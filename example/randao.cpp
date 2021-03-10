#include <platon/platon.hpp>

using namespace platon;

/**
* 1. RANDAO 的确让随机数难以预测，并且随机数享有和底层共识协议一样的活性，但攻击者可以拖到 Reveal（披露）阶段的最后再决定是否揭示
* 自己的承诺，虽然存在质押损失，但是如果收益更高还是有攻击动机的。
* 2. 可以使用 VDF 替换掉最后的那个异或运算，将其改变为执行时间必定长于各方随机数披露等待期的操作。如果计算最终结果的时间比随机数
* 披露等待期要长，那么即使是最后一刻提交的人也无法知道随机数的结果，操作结果也就无从谈起。
*
*/

//  生成随机数算法
CONTRACT Randao : public platon::Contract
{
public:
    struct Participant
    {
        std::uint256_t secret;
        std::array<uint8_t, 32> commitment;
        std::uint256_t reward;
        bool revealed;
        bool rewarded;
        PLATON_SERIALIZE(Participant, (secret)(commitment)(reward)(revealed)(rewarded))
    };

    struct Consumer
    {
        platon::Address caddr;
        platon::u128 bountypot;
        PLATON_SERIALIZE(Consumer, (caddr)(bountypot))
    };

    struct Campaign
    {
        uint64_t burn_num;
        std::uint256_t deposit;
        uint16_t commit_balkline;
        uint16_t commit_deadline;

        std::uint256_t random;
        bool settled;
        platon::u128 bountypot;
        uint32_t commit_num;
        uint32_t reveals_num;
        std::map<platon::Address, Consumer> consumers;
        std::map<platon::Address, Participant> participants;
        std::map<std::array<uint8_t, 32>, bool> commitments;
        PLATON_SERIALIZE(Campaign, (burn_num)(deposit)(commit_balkline)(commit_deadline)(random)(settled)(bountypot)(commit_num)(reveals_num)(consumers)(participants)(commitments))
    };

public:
    platon::StorageType<"numCampaigns"_n, std::uint256_t> numCampaigns;
    platon::StorageType<"campaigns"_n, std::vector<Campaign>> campaigns;
    platon::StorageType<"founder"_n, platon::Address> founder;

public:
    // campaignID, from, bnum, deposit, commitBalkline, commitDeadline, bountypot
    PLATON_EVENT2(LogCampaignAdded, std::uint256_t, platon::Address, uint64_t, std::uint256_t, uint16_t, uint16_t, platon::u128);

    // CampaignId, from, bountypot
    PLATON_EVENT2(LogFollow, std::uint256_t, platon::Address, std::uint256_t);

    // CampaignId, from, commitment
    using bytes32 = std::array<uint8_t, 32>;
    PLATON_EVENT2(LogCommit, std::uint256_t, platon::Address, bytes32);

    // CampaignId, from, secret
    PLATON_EVENT2(LogReveal, std::uint256_t, platon::Address, std::uint256_t);

public:
    void init() { founder.self() = platon_caller(); }

    /**
     * @brief 随机数需求方，想要生成随机数，首先需要新建一轮活动，需要使用 NewCampaign 函数
     * 比如当前块高度是 1840602，而我们要在 1840900 这个块需要一个随机数，希望每个参与者提交押金为 20，在目标块
     * 前 200 个块开始提交（即从 1840700 块起， 包含 1840700)， 在目标块前 100 个块结束提交（即 1840800块截止，包含 1840800块）。
     * 在 1840800 之后（不包含 1840800）1840900 块之前（不包含 1840900）  属于 Reveal 阶段，可以使用如下方式调用：
     * NewCampaign(1840700, 20000000000000000000, 200, 100)，并且可以发送作为参与者奖励费用。
     * 
     * @param burn_num 随机数生成的目标块高
     * @param deposit 参与者需要提交的押金
     * @param commit_balkline 开始提交到目标块的距离
     * @param commit_deadline 结束提交到目标块的距离
     * 
     * @return 活动 ID 号
     * 
     */
    ACTION std::uint256_t NewCampaign(uint64_t burn_num, const std::uint256_t &deposit, uint16_t commit_balkline, uint16_t commit_deadline)
    {
        // check parameter
        if (platon_block_number() >= burn_num)
            platon_revert();
        if (commit_balkline <= 0 || commit_deadline <= 0)
            platon_revert();
        if (commit_deadline > commit_balkline)
            platon_revert();
        if (platon_block_number() > burn_num - commit_balkline)
            platon_revert();
        if (deposit <= std::uint256_t(0))
            platon_revert();

        // generate a new campaign
        std::uint256_t campaign_id = campaigns.self().size() + 1;
        campaigns.self().push_back(Campaign());
        Campaign &one = campaigns.self().back();
        one.burn_num = burn_num;
        one.deposit = deposit;
        one.commit_balkline = commit_balkline;
        one.commit_deadline = commit_deadline;
        one.bountypot = platon_call_value();

        one.consumers[platon_caller()] = Consumer{platon_caller(), platon_call_value()};

        // emit event
        PLATON_EMIT_EVENT2(LogCampaignAdded, campaign_id, platon_caller(), burn_num, deposit, commit_balkline, commit_deadline, platon_call_value());

        return campaign_id;
    }

    /**
     * @brief 随机数需求方可以选择不创建一轮活动，而是选择跟随某一轮随机数活动作为自己的随机数，这时可以使用 Follow 函数
     * 跟随活动必须是在提交随机数窗口期或之前进行，否则就会失败。以前面例子为例，跟随活动必选在 1840800块之前（包括 1840800）。
     * 同样跟随活动可以发送主币作为参与者的奖励费用
     * 
     * @param campaign_id 已存在的活动 id
     * 
     * 
     */
    void Follow(std::uint256_t campaign_id)
    {
        // check parameter
        if (campaign_id > std::uint256_t(campaigns.self().size()))
            platon_revert();

        Campaign &one = campaigns.self()[int(campaign_id - std::uint256_t(1))];
        if (platon_block_number() > one.burn_num - one.commit_deadline)
            platon_revert();

        auto iter = one.consumers.find(platon_caller());
        if (iter != one.consumers.end())
            platon_revert();

        one.bountypot += platon_call_value();
        one.consumers[platon_caller()] = Consumer{platon_caller(), platon_call_value()};
        PLATON_EMIT_EVENT2(LogFollow, campaign_id, platon_caller(), platon_call_value());
    }

    /**
     * @brief 参与者可以通过提交随机数参与随机数的生成，提交随机数可以调用 Commit 函数
     * 提交随机数需要发送押金到合约，不能多于或者少于活动押金，必须刚好等于。提交随机数，必须在提交随机数窗口期提交，否则会失败。
     * 以前面的例子为例，提交随机数窗口期为： 1840700 到 1840800
     * 
     * @param campaign_id 已存在的活动 id
     * @param hs 随机数的 sh3 值
     *  
     */
    void Commit(std::uint256_t campaign_id, const bytes32 &hs)
    {
        // check parameter
        if (campaign_id > std::uint256_t(campaigns.self().size())) platon_revert();
        Campaign &one = campaigns.self()[int(campaign_id - std::uint256_t(1))];

        if(platon_block_number() < one.burn_num - one.commit_balkline) platon_revert();
        if(platon_block_number() < one.burn_num - one.commit_deadline) platon_revert();

        auto iter = one.participants.find(platon_caller());
        if(iter != one.participants.end()) platon_revert();

        auto iter_cmt = one.commitments.find(hs);
        if(iter_cmt != one.commitments.end()) platon_revert();

        one.participants[platon_caller()] = Participant{0, hs, false, false};
        one.commit_num++;
        one.commitments[hs] = true;

        PLATON_EMIT_EVENT2(LogCommit, campaign_id, platon_caller(), hs);
    }


    /**
     * @brief 披露随机数
     * 在随机数提交阶段结束之后，进入 Reveal 阶段，随机数提交者可以披露自己的随机数，合约验证是否是有效的随机数。
     * 如果有效，将计算到最终的随机数结果中，已前面的例子为例，随机数披露窗口期为： 1840800 到 1840900
     * 
     * @param campaign_id 已存在的活动 id
     * @param s 随机数
     *  
     */
    void Reveal(std::uint256_t campaign_id, std::uint256_t s)
    {
        // check parameter
        if (campaign_id > std::uint256_t(campaigns.self().size())) platon_revert();
        Campaign &cam = campaigns.self()[int(campaign_id - std::uint256_t(1))];

        auto iter = cam.participants.find(platon_caller());
        if(iter != cam.participants.end()) platon_revert();
        Participant &pat = cam.participants[platon_caller()];
        
        // check secret
        auto func = [](std::vector<uint8_t> &result, uint8_t one) { result.push_back(one); };
        std::vector<uint8_t> temp;
        s.ToBigEndian(temp, func);
        bytes32 result;
        platon_sha3(&temp[0], temp.size(), &result[0], 32);
        if(result != pat.commitment) platon_revert();

        if(pat.revealed) platon_revert();

        pat.secret = s;
        pat.revealed = true;

        cam.reveals_num++;
        cam.random ^= pat.secret;

        PLATON_EMIT_EVENT2(LogReveal, campaign_id, platon_caller(), s);
    }

    /**
     * @brief 获取随机数
     * 任何人可以在随机数目标块数之后，获取该轮活动的随机数。
     * 只有当所有的随机数提交者提交的随机数全部都收集到，才认为本轮随机数生成有效。
     * 对于没有在收集阶段提交随机数的参与者，将罚没其提交的押金，并均匀分给其他参与者
     * 
     * @param campaign_id 已存在的活动 id
     *  
     */
    std::uint256_t GetRandom(std::uint256_t campaign_id)
    {
        // check parameter
        if (campaign_id > std::uint256_t(campaigns.self().size())) platon_revert();
        Campaign &cam = campaigns.self()[int(campaign_id - std::uint256_t(1))];

        if(platon_block_number() < cam.burn_num) platon_revert();

        if(cam.reveals_num == cam.commit_num) {
            return cam.random;
        }else{
            platon_revert();
        }

        return std::uint256_t(0);
    }

    /**
     * @brief 获取奖励和押金
     * 在目标块之后，随机数提交者可以收回其押金和收益。
     * 如果随机数生成成功，将平分奖励费用，并返还押金。
     * 如果随机数生成失败，将平分未披露随机数的参与者的押金，返还披露随机数的参与者的押金
     * 如果随机数生成失败，且没有任何人成功披露随机数，所有人参与者都可以取回自己的押金
     * 
     * @param campaign_id 已存在的活动 id
     *  
     */
    std::uint256_t CalculateShare(const Campaign &cam){
        std::uint256_t result;
        if(cam.commit_num > cam.reveals_num) {
            result = std::uint256_t(cam.commit_num - cam.reveals_num) * cam.deposit / std::uint256_t(cam.reveals_num);
        }else{
            result = cam.bountypot / cam.reveals_num;
        }

        return result;
    }

    void ReturnReward(const std::uint256_t & share, const Campaign &cam, Participant &pat){
        pat.reward = share;
        pat.rewarded = true;

        uint8_t addr[20];
        platon_caller(addr);

        std::uint256_t result = share + std::uint256_t(cam.deposit);
        auto func = [](std::vector<uint8_t> &result, uint8_t one) { result.push_back(one); };
        std::vector<uint8_t> temp;
        result.ToBigEndian(temp, func);

        platon_transfer(addr, &temp[0], temp.size());
    }

    void GetMyBounty(std::uint256_t campaign_id)
    {
        // check parameter
        if (campaign_id > std::uint256_t(campaigns.self().size())) platon_revert();
        Campaign &cam = campaigns.self()[int(campaign_id - std::uint256_t(1))];

        auto iter = cam.participants.find(platon_caller());
        if(iter != cam.participants.end()) platon_revert();
        Participant &pat = cam.participants[platon_caller()];

        if(platon_block_number() < cam.burn_num) platon_revert();

        if(pat.rewarded) platon_revert();

        if(cam.reveals_num > 0){
            if(pat.revealed){
                std::uint256_t share = CalculateShare(cam);
                ReturnReward(share, cam, pat);
            }
        }else{
            ReturnReward(0, cam, pat);
        }
    }


    /**
     * @brief 退还奖金
     * 如果本轮随机数生成失败，随机数需求方可以通过 RefundBounty 函数，返还其提交的奖励
     * 
     * @param campaign_id 已存在的活动 id
     *  
     */
    void RefundBounty(std::uint256_t campaign_id)
    {
        // check parameter
        if (campaign_id > std::uint256_t(campaigns.self().size())) platon_revert();
        Campaign &cam = campaigns.self()[int(campaign_id - std::uint256_t(1))];

        if(platon_block_number() < cam.burn_num) platon_revert();

        if(cam.commit_num == cam.reveals_num && cam.commit_num != 0) platon_revert();

        if(cam.consumers[platon_caller()].caddr != platon_caller()) platon_revert();

        std::uint256_t bountypot = cam.consumers[platon_caller()].bountypot;
        cam.consumers[platon_caller()].bountypot = 0;

        uint8_t addr[20];
        platon_caller(addr);

        auto func = [](std::vector<uint8_t> &result, uint8_t one) { result.push_back(one); };
        std::vector<uint8_t> temp;
        bountypot.ToBigEndian(temp, func);
    }
};

PLATON_DISPATCH(Randao, (init)(NewCampaign)(Follow)(Commit)(Reveal)(GetRandom)(GetMyBounty)(RefundBounty))
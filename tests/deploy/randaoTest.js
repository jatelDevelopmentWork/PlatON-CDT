var contract = require("./contract");
var assert = require('assert');

async function randaoTest(){
    var randaoContract = await contract.deployContract("randao");

    await contract.actionMethodSend("randao", randaoContract, 'NewCampaign', 9200, 200000000000, 200, 100);

    contract.changeFrom("lax1ghvqzvmpxwarcqhkmd6d5f3lrzept59wvj0feg");
    await contract.actionMethodSend("randao", randaoContract, 'Follow', 1);

    // var randaoContract = "lax1mhjgfc9lqdln9xlzfytl6yur4uxq0ksrmw596t"
    // await contract.actionMethodSend("randao", randaoContract, 'Commit', 1, [0x26, 0x70, 0x0e, 0x13, 0x98, 0x3f, 0xef, 0xbd, 0x9c, 0xf1, 0x6d, 0xa2, 0xed, 0x70, 0xfa, 0x5c, 0x67, 0x98, 0xac, 0x55, 0x06, 0x2a, 0x48, 0x03, 0x12, 0x1a, 0x86, 0x97, 0x31, 0xe3, 0x08, 0xd2]);
    
    // await contract.actionMethodSend("randao", randaoContract, 'Reveal', 1, 100);

    // var random = await contract.constMethodCall("randao", randaoContract, 'GetRandom', 1);
    // console.log(random);

    // await contract.actionMethodSend("randao", randaoContract, 'GetMyBounty', 1);

    // await contract.actionMethodSend("randao", randaoContract, 'RefundBounty', 1);
}

randaoTest();
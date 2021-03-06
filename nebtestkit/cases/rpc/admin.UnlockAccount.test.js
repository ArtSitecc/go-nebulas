'use strict';

var expect = require('chai').expect;
var rpc_client = require('./rpc_client/rpc_client.js');
var Wallet = require("nebulas");

var coinbase,
    chain_id,
    sourceAccount,
    server_address;

var env = process.env.NET || 'local';
var env = 'maintest';
if (env === 'testneb1') {
  chain_id = 1001;
  sourceAccount = new Wallet.Account("25a3a441a34658e7a595a0eda222fa43ac51bd223017d17b420674fb6d0a4d52");
  coinbase = "n1SAeQRVn33bamxN4ehWUT7JGdxipwn8b17";
  server_address = "35.182.48.19:8684";

} else if (env === "testneb2") {
  chain_id = 1002;
  sourceAccount = new Wallet.Account("25a3a441a34658e7a595a0eda222fa43ac51bd223017d17b420674fb6d0a4d52");
  coinbase = "n1SAeQRVn33bamxN4ehWUT7JGdxipwn8b17";
  server_address = "34.205.26.12:8684";

} else if (env === "testneb3") {
  chain_id = 1003;
  sourceAccount = new Wallet.Account("25a3a441a34658e7a595a0eda222fa43ac51bd223017d17b420674fb6d0a4d52");
  coinbase = "n1SAeQRVn33bamxN4ehWUT7JGdxipwn8b17";
  server_address = "35.177.214.138:8684";

} else if (env === "testneb4") { //super node
  chain_id = 1004;
  sourceAccount = new Wallet.Account("c75402f6ffe6edcc2c062134b5932151cb39b6486a7beb984792bb9da3f38b9f");
  coinbase = "n1EzGmFsVepKduN1U5QFyhLqpzFvM9sRSmG";
  server_address = "35.154.108.11:8684";
} else if (env === "testneb4_normalnode"){
  chain_id = 1004;
  sourceAccount = new Wallet.Account("c75402f6ffe6edcc2c062134b5932151cb39b6486a7beb984792bb9da3f38b9f");
  coinbase = "n1EzGmFsVepKduN1U5QFyhLqpzFvM9sRSmG";
  server_address = "18.197.107.228:8684";
} else if (env === "local") {
  chain_id = 100;
  sourceAccount = new Wallet.Account("d80f115bdbba5ef215707a8d7053c16f4e65588fd50b0f83369ad142b99891b5");
  coinbase = "n1QZMXSZtW7BUerroSms4axNfyBGyFGkrh5";
  server_address = "127.0.0.1:8684";

} else if (env === "maintest"){
  chain_id = 2;
  sourceAccount = new Wallet.Account("d2319a8a63b1abcb0cc6d4183198e5d7b264d271f97edf0c76cfdb1f2631848c");
  coinbase = "n1dZZnqKGEkb1LHYsZRei1CH6DunTio1j1q";
  server_address = "54.149.15.132:8684";
} else {
  throw new Error("invalid env (" + env + ").");
}

var client,
    address;

function testUnlockAccount(testInput, testExpect, done) {
    try {
        client.UnlockAccount(testInput.args, (err, resp) => {
            try {
                expect(!!err).to.equal(testExpect.hasError);

                if (err) {
                    console.log("call return error: " + JSON.stringify(err));
                    expect(err).have.property('details').equal(testExpect.errorMsg);
                } else {
                    console.log("call return success: " + JSON.stringify(resp));
                    expect(resp).to.have.property('result').equal(testExpect.result);
                }
                done();
            } catch (err) {
                done(err);
            }
        });
    } catch(err) {
        console.log("call failed:" + err.toString())
        if (testExpect.callFailed) {
            try {
                expect(err.toString()).to.have.string(testExpect.errorMsg);
                done();
            } catch(er) {
                done(er);
            }
        } else {
            done(err)
        }
    }
}

describe("rpc: UnlockAccount", () => {
    before((done) => {
        client = rpc_client.new_client(server_address, 'AdminService');

        client.NewAccount({passphrase: "passphrase"}, (err, resp) => {
            try {
                expect(!!err).to.be.false;
                expect(resp).to.have.property('address');
                address = resp.address;
                console.log("create new account: " + address);
                done();
            } catch(err) {
                done(err)
            }
        });
    });

    it("1. normal", (done) => {
        var testInput = {
            args: {
                address: address,
                passphrase: "passphrase",
            }
        }

        var testExpect = {
            result: true,
            hasError: false
        }

        testUnlockAccount(testInput, testExpect, done);
    });

    it("2. wrong `passphrase`", (done) => {
        var testInput = {
            args: {
                address: address,
                passphrase: "wrwrwqweqw"
            }
        }

        var testExpect = {
            hasError: true,
            errorMsg: "could not decrypt key with given passphrase"
        }

        testUnlockAccount(testInput, testExpect, done);
    });

    it("3. empty `passphrase`", (done) => {
        var testInput = {
            args: {
                address: address,
                passphrase: ""
            }
        }

        var testExpect = {
            hasError: true,
            errorMsg: "passphrase is invalid"
        }

        testUnlockAccount(testInput, testExpect, done);
    });

    it("4. nonexistent `address`", (done) => {
        var testInput = {
            args: {
                address: Wallet.Account.NewAccount().getAddressString(),
                passphrase: "passphrase"
            }
        }

        var testExpect = {
            hasError: true,
            errorMsg: "account is not found"
        }

        testUnlockAccount(testInput, testExpect, done);
    });

    it("5. invalid `address`", (done) => {
        var testInput = {
            args: {
                address: "eb31ad2d8a89a0ca693425730430bc2d63f2573b8",   // same with ""
                passphrase: "passphrase",
            }
        }

        var testExpect = {
            hasError: true,
            errorMsg: "address: invalid address format"
        }

        testUnlockAccount(testInput, testExpect, done);
    });

    it("6. missing `address`", (done) => {
        var testInput = {
            args: {
                // address: address,
                passphrase: "passphrase"
            }
        }

        var testExpect = {
            hasError: true,
            errorMsg: 'address: invalid address format'
        }

        testUnlockAccount(testInput, testExpect, done);
    });

    it("7. missing `passphrase`", (done) => {
        var testInput = {
            args: {
                address: address,
                // passphrase: "passphrase"
            }
        }

        var testExpect = {
            hasError: true,
            errorMsg: 'passphrase is invalid'
        }

        testUnlockAccount(testInput, testExpect, done);
    });

    it("8. redundant param", (done) => {
        var testInput = {
            args: {
                address: address,
                passphrase: "passphrase",
                test: "gtes"
            }
        }

        var testExpect = {
            callFailed: true,
            errorMsg: 'Error:'
        }

        testUnlockAccount(testInput, testExpect, done);
    });
});
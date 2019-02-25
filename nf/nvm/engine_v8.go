// Copyright (C) 2017 go-nebulas authors
//
// This file is part of the go-nebulas library.
//
// the go-nebulas library is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// the go-nebulas library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with the go-nebulas library.  If not, see <http://www.gnu.org/licenses/>.
//

package nvm

import (
	"fmt"
	"strings"
	"sync"
	"errors"
	"time"
	"strconv"

	"encoding/json"
	"golang.org/x/net/context"

	lru "github.com/hashicorp/golang-lru"
	"github.com/nebulasio/go-nebulas/core"
	"github.com/nebulasio/go-nebulas/util/logging"
	"github.com/sirupsen/logrus"
	"google.golang.org/grpc"
)

const (
	ExecutionFailedErr  = 1
	ExecutionTimeOutErr = 2

	// ExecutionTimeoutInSeconds max v8 execution timeout.
	ExecutionTimeout                 = 15 * 1000 * 1000
	TimeoutGasLimitCost              = 100000000
	MaxLimitsOfExecutionInstructions = 10000000 // 10,000,000

	NVMDataExchangeTypeStart = "start"
	NVMDataExchangeTypeCallBack = "callback"
	NVMDataExchangeTypeFinal = "final"

	NVM_SUCCESS = 0
	NVM_EXCEPTION_ERR = -1
	NVM_MEM_LIMIT_ERR = -2
	NVM_GAS_LIMIT_ERR = -3
	NVM_UNEXPECTED_ERR = -4
	NVM_EXE_TIMEOUT_ERR = -5
	NVM_TRANSPILE_SCRIPT_ERR = -6
	NVM_INJECT_TRACING_INSTRUCTION_ERR = -7
)

// callback function names
const (
	REQUIRE_DELEGATE_FUNC = "RequireDelegateFunc"
	ATTACH_LIB_VERSION_DELEGATE_FUNC = "AttachLibVersionDelegateFunc"
	STORAGE_GET = "StorageGet"
	STORAGE_PUT = "StoragePut"
	STORAGE_DEL = "StorageDel"
	GET_TX_BY_HASH = "GetTxByHash"
	GET_ACCOUNT_STATE = "GetAccountState"
	TRANSFER = "Transfer"
	VERIFY_ADDR = "VerifyAddress"
	GET_PRE_BLOCK_HASH = "GetPreBlockHash"
	GET_PRE_BLOCK_SEED = "GetPreBlockSeed"
	EVENT_TRIGGER_FUNC = "EventTriggerFunc"
	SHA_256_FUNC = "Sha256Func"
	SHA_3256_FUNC = "Sha3256Func"
	RIPEMD_160_FUNC = "Ripemd160Func"
	RECOVER_ADDRESS_FUNC = "RecoverAddressFunc"
	MD5_FUNC = "Md5Func"
	BASE64_FUNC = "Base64Func"	
)

//engine_v8 private data
var (
//	v8engineOnce         = sync.Once{}
	storages             = make(map[uint64]*V8Engine, 1024)
	storagesIdx          = uint64(0)
	storagesLock         = sync.RWMutex{}
//	engines              = make(map[*C.V8Engine]*V8Engine, 1024)
//	enginesLock          = sync.RWMutex{}
	sourceModuleCache, _ = lru.New(40960)
	inject               = 0
	hit                  = 0
	nvmRequestIndex uint32	 = 1
)

// V8Engine v8 engine.
type V8Engine struct {
	ctx                                     *Context
	modules                                 Modules
	//v8engine                                *C.V8Engine
	strictDisallowUsageOfInstructionCounter int
	enableLimits                            bool
	limitsOfExecutionInstructions           uint64
	limitsOfTotalMemorySize                 uint64
	actualCountOfExecutionInstructions      uint64
	actualTotalMemorySize                   uint64
	lcsHandler                              uint64
	gcsHandler                              uint64
	serverListenAddr						string
	startExeTime							time.Time
	executionTimeOut						uint64
}

type sourceModuleItem struct {
	source                    string
	sourceLineOffset          int
	traceableSource           string
	traceableSourceLineOffset int
}

// NewV8Engine return new V8Engine instance.
func NewV8Engine(ctx *Context) *V8Engine {

	engine := &V8Engine{
		ctx:      ctx,
		modules:  NewModules(),
		strictDisallowUsageOfInstructionCounter: 1, // enable by default.
		enableLimits:                            true,
		limitsOfExecutionInstructions:           0,
		limitsOfTotalMemorySize:                 0,
		actualCountOfExecutionInstructions:      0,
		actualTotalMemorySize:                   0,
		executionTimeOut:			  			 0,
	}

	/*
	(func() {
		enginesLock.Lock()
		defer enginesLock.Unlock()
		engines[engine.v8engine] = engine
	})()
	*/

	(func() {
		storagesLock.Lock()
		defer storagesLock.Unlock()

		storagesIdx++
		engine.lcsHandler = storagesIdx
		storagesIdx++
		engine.gcsHandler = storagesIdx

		storages[engine.lcsHandler] = engine
		storages[engine.gcsHandler] = engine
	})()

	if ctx.block.Height() >= core.NvmGasLimitWithoutTimeoutAtHeight {
		engine.executionTimeOut = ExecutionTimeout		// set to max
	}

	return engine
}

// SetEnableLimit eval switch
func (e *V8Engine) SetEnableLimit(isLimit bool) {
	e.enableLimits = isLimit
}

// Dispose dispose all resources.
func (e *V8Engine) Dispose() {
	storagesLock.Lock()
	delete(storages, e.lcsHandler)
	delete(storages, e.gcsHandler)
	storagesLock.Unlock()
}

// Context returns engine context
func (e *V8Engine) Context() *Context {
	return e.ctx
}

// SetExecutionLimits set execution limits of V8 Engine, prevent Halting Problem.
func (e *V8Engine) SetExecutionLimits(limitsOfExecutionInstructions, limitsOfTotalMemorySize uint64) error {

	logging.VLog().WithFields(logrus.Fields{
		"limits_of_executed_instructions": limitsOfExecutionInstructions,
		"limits_of_total_memory_size":     limitsOfTotalMemorySize,
	}).Debug("set execution limits.")

	e.limitsOfExecutionInstructions = limitsOfExecutionInstructions
	e.limitsOfTotalMemorySize = limitsOfTotalMemorySize

	if limitsOfExecutionInstructions == 0 || limitsOfTotalMemorySize == 0 {
		logging.VLog().Debugf("limit args has empty. limitsOfExecutionInstructions:%v,limitsOfTotalMemorySize:%d", limitsOfExecutionInstructions, limitsOfTotalMemorySize)
		return ErrLimitHasEmpty
	}
	// V8 needs at least 6M heap memory.
	if limitsOfTotalMemorySize < 6000000 {
		logging.VLog().Debugf("V8 needs at least 6M (6000000) heap memory, your limitsOfTotalMemorySize (%d) is too low.", limitsOfTotalMemorySize)
		return ErrSetMemorySmall
	}
	return nil
}

// ExecutionInstructions returns the execution instructions
func (e *V8Engine) ExecutionInstructions() uint64 {
	return e.actualCountOfExecutionInstructions
}

func (e *V8Engine) CheckTimeout() bool {
	elapsedTime := time.Since(e.startExeTime)

	if elapsedTime.Nanoseconds()/1000 > ExecutionTimeout{
		logging.CLog().WithFields(logrus.Fields{
				"elapsedTime": elapsedTime,
			}).Error("NVM execution timed out.")
		return true
	}

	return false
}

// Call function in a script
func (e *V8Engine) Call(config *core.NVMConfig, listenAddr string) (string, error) {
	e.serverListenAddr = listenAddr

	function := config.FunctionName
	if core.PublicFuncNameChecker.MatchString(function) == false {
		logging.VLog().Debugf("Invalid function: %v", function)
		return "", ErrDisallowCallNotStandardFunction
	}
	if strings.EqualFold(core.ContractInitFunc, function) == true {
		return "", ErrDisallowCallPrivateFunction
	}
	return e.RunScriptSource(config)
}

func (e *V8Engine) DeployAndInit(config *core.NVMConfig, listenAddr string) (string, error){
	e.serverListenAddr = listenAddr
	config.FunctionName = core.ContractInitFunc
	return e.RunScriptSource(config)
}

func (e *V8Engine) RunScriptSource(config *core.NVMConfig) (string, error){

	// check source type
	sourceType := config.PayloadSourceType
	if sourceType != core.SourceTypeJavaScript && sourceType != core.SourceTypeTypeScript {
		return "", ErrUnsupportedSourceType
	}

	// prepare for execute.
	block := toSerializableBlock(e.ctx.block)
	blockJSON, err := json.Marshal(block)
	if err != nil {
		return "", errors.New("Failed to serialize block")
	}
	tx := toSerializableTransaction(e.ctx.tx)
	txJSON, err := json.Marshal(tx)
	if err != nil {
		return "", errors.New("Failed to serialize transaction")
	}

	//var runnableSource string
	var argsInput []byte
	args := config.GetContractArgs()
	if len(args) > 0 {
		var argsObj []interface{}
		if err := json.Unmarshal([]byte(args), &argsObj); err != nil {
			return "", errors.New("Arguments error")
		}
		if argsInput, err = json.Marshal(argsObj); err != nil {
			return "", errors.New("Arguments error")
		}
	} else {
		argsInput = []byte("[]")
	}

	moduleID := "contract.js"			// for module recognition
	runnableSource := fmt.Sprintf(`Blockchain.blockParse("%s");
		Blockchain.transactionParse("%s");
		var __contract = require("%s");
		var __instance = new __contract();
		__instance["%s"].apply(__instance, JSON.parse("%s"));`,
			formatArgs(string(blockJSON)), formatArgs(string(txJSON)),
			moduleID, config.FunctionName, formatArgs(string(argsInput)))

	// check height settings carefully
	/*
	if e.ctx.block.Height() >= core.NvmMemoryLimitWithoutInjectHeight {
		//TODO: collect tracing stats
		mem := e.actualTotalMemorySize + core.DefaultLimitsOfTotalMemorySize
		logging.VLog().WithFields(logrus.Fields{
			"actualTotalMemorySize": e.actualTotalMemorySize,
			"limit":                 core.DefaultLimitsOfTotalMemorySize,
			"tx.hash":               e.ctx.tx.Hash(),
		}).Debug("mem limit")
		e.limitsOfTotalMemorySize = mem
	}
	*/

	if e.ctx.block.Height() >= core.NvmGasLimitWithoutTimeoutAtHeight {
		if e.limitsOfExecutionInstructions > MaxLimitsOfExecutionInstructions {
			e.limitsOfExecutionInstructions = MaxLimitsOfExecutionInstructions
		}
	}

	// Send request
	conn, err := grpc.Dial(e.serverListenAddr, grpc.WithInsecure())
	if err != nil {
		logging.VLog().WithFields(logrus.Fields{
			"err": err,
		}).Error("Failed to connect with V8 server")

		// try to re-launch the process
		
	}
	defer conn.Close()

	logging.CLog().Info("NVM client is trying to connect the server")
	
	v8Client := NewNVMServiceClient(conn)
	var timeOut time.Duration = 15000   // Set execution timeout to be 15s'
	ctx, cancel := context.WithTimeout(context.Background(), timeOut*time.Second)
	defer cancel()
	
	logging.CLog().Info(">>>>>>>>>Now started call request!, the listener address is: ", e.serverListenAddr)

	configBundle := &NVMConfigBundle{ScriptSrc:config.PayloadSource, ScriptType:config.PayloadSourceType, EnableLimits: true, RunnableSrc: runnableSource, 
		MaxLimitsOfExecutionInstruction:MaxLimitsOfExecutionInstructions, DefaultLimitsOfTotalMemSize:core.DefaultLimitsOfTotalMemorySize,
		LimitsExeInstruction: e.limitsOfExecutionInstructions, LimitsTotalMemSize: e.limitsOfTotalMemorySize, ExecutionTimeout: e.executionTimeOut,
		BlockJson:formatArgs(string(blockJSON)), TxJson: formatArgs(string(txJSON)), ModuleId: moduleID}

	callbackResult := &NVMCallbackResult{Result:""}

	// for call request, the metadata is nil
	request := &NVMDataRequest{
		RequestType:NVMDataExchangeTypeStart, 
		RequestIndx:nvmRequestIndex,
		ConfigBundle: configBundle,
		LcsHandler: e.lcsHandler, 
		GcsHandler: e.gcsHandler,
		CallbackResult: callbackResult}

	stream, err := v8Client.SmartContractCall(ctx); if err != nil {
		logging.VLog().WithFields(logrus.Fields{
			"err": err,
			"module": "nvm",
		}).Error("Failed to get streaming object")
		return "", ErrRPCConnection
	}

	err = stream.Send(request); if err != nil {
		logging.VLog().WithFields(logrus.Fields{
			"err": err,
			"module": "nvm",
		}).Error("Failed to send out initial request")
		return "", ErrRPCConnection
	}

	// start counting for execution
	e.startExeTime = time.Now()
	var counter uint32 = 1		// for debugging purpose
	for {
		dataResponse, err := stream.Recv()
		if err != nil{
			logging.VLog().WithFields(logrus.Fields{
				"err": err,
				"module": "nvm",
			}).Error("Failed to receive data response from server")
			return "", ErrRPCConnection
		}

		logging.CLog().WithFields(logrus.Fields{
			"response_type": dataResponse.GetResponseType(),
			"response_indx": dataResponse.GetResponseIndx(),
			"module": "nvm",
		}).Info(">>>>>>>Receiving a call back from the v8 process to handle")

		if(dataResponse.GetResponseType() == NVMDataExchangeTypeFinal && dataResponse.GetFinalResponse() != nil){

			stream.CloseSend()
			finalResponse := dataResponse.GetFinalResponse()
			ret := finalResponse.Result
			result := finalResponse.Msg
			stats := finalResponse.StatsBundle

			// check the result here
			logging.CLog().WithFields(
				logrus.Fields{
					"result": result,                                                                                                                                                                                                                                                                                                     
					"ret": ret,
				}).Info(">>>>The contract execution result")
		
			// TODO, collect tracing stats
			//e.CollectTracingStats()
			actualCountOfExecutionInstructions := stats.ActualCountOfExecutionInstruction
			actualUsedMemSize := stats.ActualUsedMemSize
			e.actualCountOfExecutionInstructions = actualCountOfExecutionInstructions
			e.actualTotalMemorySize = actualUsedMemSize

			logging.CLog().WithFields(logrus.Fields{
				"actualAcountOfExecutionInstructions": actualCountOfExecutionInstructions,
				"actualUsedMemSize": actualUsedMemSize,
				"finalresult": ret,
				"tx hash": e.ctx.tx.Hash(),
				"tx height": e.ctx.block.Height(),
			}).Info(">>>>Got stats info")
		
			if e.ctx.block.Height() >= core.NvmGasLimitWithoutTimeoutAtHeight {
				if e.limitsOfExecutionInstructions == MaxLimitsOfExecutionInstructions && err == ErrInsufficientGas {
				  err = ErrExecutionTimeout
				  result = "\"null\""
				}
			}

			//set err
			if ret == NVM_TRANSPILE_SCRIPT_ERR {
				return result, ErrTranspileTypeScriptFailed

			} else if ret == NVM_INJECT_TRACING_INSTRUCTION_ERR {
				return result, ErrInjectTracingInstructionFailed

			} else if ret == NVM_EXE_TIMEOUT_ERR {
				err = ErrExecutionTimeout
				if e.ctx.block.Height() >= core.NvmGasLimitWithoutTimeoutAtHeight {
					err = core.ErrUnexpected
				} else if e.ctx.block.Height() >= core.NewNvmExeTimeoutConsumeGasHeight {
					if TimeoutGasLimitCost > e.limitsOfExecutionInstructions {
						e.actualCountOfExecutionInstructions = e.limitsOfExecutionInstructions

						//actualCountOfExecutionInstructions = e.limitsOfExecutionInstructions

					} else {
						e.actualCountOfExecutionInstructions = TimeoutGasLimitCost
						
						//actualCountOfExecutionInstructions = TimeoutGasLimitCost
					}
				}
			} else if ret == NVM_UNEXPECTED_ERR {
				err = core.ErrUnexpected
			} else {
				if ret != NVM_SUCCESS {
					err = core.ErrExecutionFailed
				}
				if e.limitsOfExecutionInstructions > 0 &&
					e.limitsOfExecutionInstructions < e.actualCountOfExecutionInstructions {
					// Reach instruction limits.
					err = ErrInsufficientGas
					e.actualCountOfExecutionInstructions = e.limitsOfExecutionInstructions
				} else if e.limitsOfTotalMemorySize > 0 && e.limitsOfTotalMemorySize < e.actualTotalMemorySize {
					// reach memory limits.
					err = ErrExceedMemoryLimits
					e.actualCountOfExecutionInstructions = e.limitsOfExecutionInstructions
				}
			}
			return result, nil

		}else{
			//TODO start to handle the callback and send result back to server
			serverLcsHandler := dataResponse.GetLcsHandler()
			serverGcsHandler := dataResponse.GetGcsHandler()
			callbackResponse := dataResponse.GetCallbackResponse()
			responseFuncName := callbackResponse.GetFuncName()
			responseFuncParams := callbackResponse.GetFuncParams()
			
			logging.CLog().WithFields(logrus.Fields{
				"response_type": dataResponse.GetResponseType,
				"response_indx": dataResponse.GetResponseIndx,
				"response_function_name": dataResponse.GetCallbackResponse().FuncName,
				"response_function_para": responseFuncParams[0],
				"module": "nvm",
			}).Info(">>>>>>>Now is receiving a call back from the v8 process to handle")

			// check the callback type
			callbackResult := &NVMCallbackResult{}
			callbackResult.FuncName = responseFuncName

			if responseFuncName == ATTACH_LIB_VERSION_DELEGATE_FUNC{
				pathName := AttachLibVersionDelegateFunc(serverLcsHandler, responseFuncParams[0])
				callbackResult.Result = pathName

			}else if responseFuncName == STORAGE_GET {
				value, gasCnt := StorageGetFunc(serverLcsHandler, responseFuncParams[0])
				callbackResult.Result = value
				callbackResult.Extra = append(callbackResult.Extra, fmt.Sprintf("%v", gasCnt))

			} else if responseFuncName == STORAGE_PUT {
				resCode, gasCnt := StoragePutFunc(serverLcsHandler, responseFuncParams[0], responseFuncParams[1])
				callbackResult.Result = fmt.Sprintf("%v", resCode)
				callbackResult.Extra = append(callbackResult.Extra, fmt.Sprintf("%v", gasCnt))

			} else if responseFuncName == STORAGE_DEL {
				resCode, gasCnt := StorageDelFunc(serverLcsHandler, responseFuncParams[0])
				callbackResult.Result = fmt.Sprintf("%v", resCode)
				callbackResult.Extra = append(callbackResult.Extra, fmt.Sprintf("%v", gasCnt))

			} else if responseFuncName == GET_TX_BY_HASH {
				resStr, gasCnt := GetTxByHashFunc(serverLcsHandler, responseFuncParams[0])
				callbackResult.Result = resStr
				callbackResult.Extra = append(callbackResult.Extra, fmt.Sprintf("%v", gasCnt))

			} else if responseFuncName == GET_ACCOUNT_STATE {
				resCode, resStr, exceptionInfo, gasCnt := GetAccountStateFunc(serverLcsHandler, responseFuncParams[0])
				callbackResult.Result = fmt.Sprintf("%v", resCode)
				callbackResult.Extra = append(callbackResult.Extra, resStr)
				callbackResult.Extra = append(callbackResult.Extra, exceptionInfo)
				callbackResult.Extra = append(callbackResult.Extra, fmt.Sprintf("%v", gasCnt))

			} else if responseFuncName == TRANSFER {
				resCode, gasCnt := TransferFunc(serverLcsHandler, responseFuncParams[0], responseFuncParams[1])
				callbackResult.Result = fmt.Sprintf("%v", resCode)
				callbackResult.Extra = append(callbackResult.Extra, fmt.Sprintf("%v", gasCnt))

			} else if responseFuncName == VERIFY_ADDR {
				resCode, gasCnt := VerifyAddressFunc(serverLcsHandler, responseFuncParams[0])
				callbackResult.Result = fmt.Sprintf("%v", resCode)
				callbackResult.Extra = append(callbackResult.Extra, fmt.Sprintf("%v", gasCnt))

			} else if responseFuncName == GET_PRE_BLOCK_HASH {
				offset, _ := strconv.ParseInt(responseFuncParams[0], 10, 64)
				resCode, resStr, exceptionInfo, gasCnt := GetPreBlockHashFunc(serverLcsHandler, uint64(offset))
				callbackResult.Result = fmt.Sprintf("%v", resCode)
				callbackResult.Extra = append(callbackResult.Extra, resStr)
				callbackResult.Extra = append(callbackResult.Extra, exceptionInfo)
				callbackResult.Extra = append(callbackResult.Extra, fmt.Sprintf("%v", gasCnt))

			} else if responseFuncName == GET_PRE_BLOCK_SEED {
				offset, _ := strconv.ParseInt(responseFuncParams[0], 10, 64)
				resCode, resStr, exceptionInfo, gasCnt := GetPreBlockSeedFunc(serverLcsHandler, uint64(offset))
				callbackResult.Result = fmt.Sprintf("%v", resCode)
				callbackResult.Extra = append(callbackResult.Extra, resStr)
				callbackResult.Extra = append(callbackResult.Extra, exceptionInfo)
				callbackResult.Extra = append(callbackResult.Extra, fmt.Sprintf("%v", gasCnt))				

			} else if responseFuncName == EVENT_TRIGGER_FUNC {
				gasCnt := EventTriggerFunc(serverLcsHandler, responseFuncParams[0], responseFuncParams[1])
				callbackResult.Result = fmt.Sprintf("%v", gasCnt)

			} else if responseFuncName == SHA_256_FUNC {
				resStr, gasCnt := Sha256Func(responseFuncParams[0])
				callbackResult.Result = resStr
				callbackResult.Extra = append(callbackResult.Extra, fmt.Sprintf("%v", gasCnt))

			} else if responseFuncName == SHA_3256_FUNC {
				resStr, gasCnt := Sha3256Func(responseFuncParams[0])
				callbackResult.Result = resStr
				callbackResult.Extra = append(callbackResult.Extra, fmt.Sprintf("%v", gasCnt))

			} else if responseFuncName == RIPEMD_160_FUNC {
				resStr, gasCnt := Ripemd160Func(responseFuncParams[0])
				callbackResult.Result = resStr
				callbackResult.Extra = append(callbackResult.Extra, fmt.Sprintf("%v", gasCnt))

			} else if responseFuncName == RECOVER_ADDRESS_FUNC {
				alg, _ := strconv.Atoi(responseFuncParams[0])
				resStr, gasCnt := RecoverAddressFunc(alg, responseFuncParams[1], responseFuncParams[2])
				callbackResult.Result = resStr
				callbackResult.Extra = append(callbackResult.Extra, fmt.Sprintf("%v", gasCnt))

			} else if responseFuncName == MD5_FUNC {
				resStr, gasCnt := Md5Func(responseFuncParams[0])
				callbackResult.Result = resStr
				callbackResult.Extra = append(callbackResult.Extra, fmt.Sprintf("%v", gasCnt))

			} else if responseFuncName == BASE64_FUNC {
				resStr, gasCnt := Base64Func(responseFuncParams[0])
				callbackResult.Result = resStr
				callbackResult.Extra = append(callbackResult.Extra, fmt.Sprintf("%v", gasCnt))

			} else {
				logging.CLog().WithFields(logrus.Fields{
						"func": responseFuncName,
						"params": responseFuncParams,
				}).Error("Invalid callback function name")
			}

			// stream.Send()
			nvmRequestIndex += 1
			newRequest := &NVMDataRequest{
				RequestType:NVMDataExchangeTypeCallBack, 
				RequestIndx:dataResponse.GetResponseIndx(),
				LcsHandler:serverLcsHandler,
				GcsHandler:serverGcsHandler,
				CallbackResult:callbackResult}

			stream.Send(newRequest)
		}
		
		if(e.CheckTimeout()){
			logging.CLog().Info("+++++++++= Now is timeout!!!!")
			return "", ErrExecutionTimeout
			break
		}
		counter += 1
		logging.CLog().Info("Counter is: ", counter)
	}

	return "", ErrUnexpected
}

func getEngineByStorageHandler(handler uint64) (*V8Engine, Account) {
	storagesLock.RLock()
	engine := storages[handler]
	storagesLock.RUnlock()

	if engine == nil {
		logging.VLog().WithFields(logrus.Fields{
			"wantedHandler": handler,
		}).Error("wantedHandler is not found.")
		return nil, nil
	}

	if engine.lcsHandler == handler {
		return engine, engine.ctx.contract
	} else if engine.gcsHandler == handler {
		// disable gcs according to issue https://github.com/nebulasio/go-nebulas/issues/23.
		return nil, nil
		// return engine, engine.ctx.owner
	} else {
		logging.VLog().WithFields(logrus.Fields{
			"lcsHandler":    engine.lcsHandler,
			"gcsHandler":    engine.gcsHandler,
			"wantedHandler": handler,
		}).Error("in-consistent storage handler.")
		return nil, nil
	}
}

// Still use the storage maps to get the v8 engine
func getEngineByEngineHandler(handler uint64) *V8Engine {
	storagesLock.RLock()
	defer storagesLock.RUnlock()

	engine := storages[handler]
	if engine == nil {
		logging.VLog().WithFields(logrus.Fields{
			"wantedHandler": handler,
		}).Error("wantedHandler is not found.")
		return nil
	}

	// only use the lcs handler to check
	if engine.lcsHandler == handler {
		return engine
	} else {
		return nil
	}
}

/*
func getEngineByEngineHandler(handler unsafe.Pointer) *V8Engine {
	v8engine := (*C.V8Engine)(handler)
	enginesLock.RLock()
	defer enginesLock.RUnlock()

	return engines[v8engine]
}
*/

func formatArgs(s string) string {
	s = strings.Replace(s, "\\", "\\\\", -1)
	s = strings.Replace(s, "\n", "\\n", -1)
	s = strings.Replace(s, "\r", "\\r", -1)
	s = strings.Replace(s, "\"", "\\\"", -1)
	return s
}
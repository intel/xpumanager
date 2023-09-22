/*
 *  Copyright (C) 2021-2023 Intel Corporation
 *  SPDX-License-Identifier: MIT
 *  @file winipmi.c
 */
#include "pch.h"
#define CINTERFACE
#ifdef _WIN32
#include "winipmi.h"
#include <stdbool.h>
#include <stdio.h>
#include "wintypes.h"

#include "infrastructure/logger.h"

VARIANT *variantList[MAX_VARIANT_LIST_SIZE];
int varListIndex = 0;


void ipmi_close_win(int ipmiDevice)
{
	ipmiDevice = -1;
	ipmi_clean_win();
	return;
}

int ipmi_open_win()
{
	HRESULT hres;
	BSTR bstrWmi = SysAllocString(L"ROOT\\WMI");
	BSTR bstrIpmi = SysAllocString(L"Microsoft_IPMI");
	BSTR bstrClassProp = SysAllocString(L"__Relpath");
	int retval = -1;
	bool n = false;
	ULONG pReturned = NULL;

	do
	{
		hres = CoInitializeEx(NULL, COINIT_MULTITHREADED);
		if (FAILED(hres))
		{
			XPUM_LOG_WARN("CoInitializeEx Failure: %0x\n", hres);
			ipmi_clean_win();
			break;
		}

		hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
			IID_IWbemLocator, (LPVOID*)&pLoc);
		if (FAILED(hres))
		{
            XPUM_LOG_WARN("CoCreateInstance Failure: %0x\n", hres);
			ipmi_clean_win();
			break;
		}

		hres = pLoc->lpVtbl->ConnectServer(pLoc, bstrWmi, NULL, NULL, 0, NULL, 0, 0, &pSvc);
		if (FAILED(hres))
		{
            XPUM_LOG_WARN("ConnectServer Failure: %0x\n", hres);
			ipmi_clean_win();
			break;
		}

		hres = CoSetProxyBlanket(reinterpret_cast<::IUnknown*> (pSvc), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
			RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
		if (FAILED(hres))
		{
            XPUM_LOG_WARN("CoSetProxyBlanket Failure: %0x\n", hres);
			ipmi_clean_win();
			break;
		}

		hres = pSvc->lpVtbl->GetObject(pSvc, bstrIpmi, WBEM_FLAG_RETURN_WBEM_COMPLETE, NULL,
			&pClass, NULL);
		if (FAILED(hres))
		{
            XPUM_LOG_WARN("GetObject Failure: %0x\n", hres);
			ipmi_clean_win();
			break;
		}

		hres = pSvc->lpVtbl->CreateInstanceEnum(pSvc, bstrIpmi,
			WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnum);
		if (FAILED(hres))
		{
            XPUM_LOG_WARN("CreateInstanceEnum Failure: %0x\n", hres);
			ipmi_clean_win();
			break;
		}

		hres = pEnum->lpVtbl->Next(pEnum, WBEM_INFINITE, 1, &pObject, &pReturned);
		if (FAILED(hres))
		{
            XPUM_LOG_WARN("Next Enum Object Failure: %0x\n", hres);
			ipmi_clean_win();
			break;
		}

		VariantInit(&varPath);
		hres = pObject->lpVtbl->Get(pObject, bstrClassProp, 0, &varPath, NULL, 0);
		if (FAILED(hres))
		{
            XPUM_LOG_WARN("Get Property Value Failure: %0x\n", hres);
			ipmi_clean_win();
			break;
		}

		retval = 0;
		ipmiOpen = 1;
	} while (n);

	SysFreeString(bstrWmi);
	SysFreeString(bstrIpmi);
	SysFreeString(bstrClassProp);
	return retval;
}

void ipmi_clean_win()
{
	VariantClear(&varPath);

	if (pObject != NULL)
	{
		pObject->lpVtbl->Release(pObject);
		pObject = NULL;
	}

	if (pEnum != NULL)
	{
		pEnum->lpVtbl->Release(pEnum);
		pEnum = NULL;
	}

	if (pClass != NULL)
	{
		pClass->lpVtbl->Release(pClass);
		pClass = NULL;
	}

	if (pSvc != NULL)
	{
		pSvc->lpVtbl->Release(pSvc);
		pSvc = NULL;
	}

	if (pLoc != NULL)
	{
		pLoc->lpVtbl->Release(pLoc);
		pLoc = NULL;
	}

	CoUninitialize();
	ipmiOpen = 0;
}

int ipmi_cmd_win(struct ipmi_req *req, struct ipmi_ipmb_addr *req_addr, struct ipmi_recv *res)
{
	int retval;
	if (!ipmiOpen)
	{
		retval = ipmi_open_win();
		if (retval != 0)
		{
			return retval;
		}
	}

	HRESULT hres;
	IWbemClassObject *pInReq = NULL;
	IWbemClassObject *pOutRecv = NULL;
	bool n = false;
	VARIANT varCmd, varNetfn, varLun, varSa, varSize, varData;
	LPCWSTR lpcwReqResp = L"RequestResponse";
	BSTR bstrCommand = SysAllocString(L"Command");
	BSTR bstrNetfn = SysAllocString(L"NetworkFunction");
	BSTR bstrLun = SysAllocString(L"Lun");
	BSTR bstrRespAddr = SysAllocString(L"ResponderAddress");
	BSTR bstrReqDataSize = SysAllocString(L"RequestDataSize");
	BSTR bstrReqData = SysAllocString(L"RequestData");
	BSTR bstrReqResp = SysAllocString(L"RequestResponse");
	BSTR bstrCompCode = SysAllocString(L"CompletionCode");
	BSTR bstrRespDataSize = SysAllocString(L"ResponseDataSize");
	BSTR bstrRespData = SysAllocString(L"ResponseData");
	uchar *pRespData = NULL;
	SAFEARRAY *psa = NULL;

	retval = -1;
    
    char ss[1023];
    memset(ss, 0, 1023);
    for (int i = 0; i < req->msg.data_len; ++i) {
     snprintf(ss + i*3, 1022,"%02x ", (int)req->msg.data[i]);
    }
    XPUM_LOG_INFO("ipmi_cmd_win req %d, %s\n", req->msg.data_len, ss);
 
	do
	{
		hres = pClass->lpVtbl->GetMethod(pClass, lpcwReqResp, 0, &pInReq, NULL);
		if (FAILED(hres))
		{
			XPUM_LOG_WARN("GetMethod Failure: %d\n", hres);
			break;
		}

		handle_variant(&varCmd);
		varCmd.vt = VT_UI1;
		varCmd.bVal = req->msg.cmd;
		hres = pInReq->lpVtbl->Put(pInReq, bstrCommand, 0, &varCmd, 0);
		if (FAILED(hres))
		{
            XPUM_LOG_WARN("Put Command Failure: %d\n", hres);
			break;
		}

		handle_variant(&varNetfn);
		varNetfn.vt = VT_UI1;
		varNetfn.bVal = req->msg.netfn;
		hres = pInReq->lpVtbl->Put(pInReq, bstrNetfn, 0, &varNetfn, 0);
		if (FAILED(hres))
		{
            XPUM_LOG_WARN("Put NetworkFunction Failure: %d\n", hres);
			break;
		}

		handle_variant(&varLun);
		varLun.vt = VT_UI1;
		varLun.bVal = req_addr->lun;
		hres = pInReq->lpVtbl->Put(pInReq, bstrLun, 0, &varLun, 0);
		if (FAILED(hres))
		{
            XPUM_LOG_WARN("Put Lun Failure: %d\n", hres);
			break;
		}

		handle_variant(&varSa);
		varSa.vt = VT_UI1;
		varSa.bVal = req_addr->slave_addr;
		hres = pInReq->lpVtbl->Put(pInReq, bstrRespAddr, 0, &varSa, 0);
		if (FAILED(hres))
		{
			XPUM_LOG_WARN("Put ResponderAddress Failure: %d\n", hres);
			break;
		}

		handle_variant(&varSize);
		varSize.vt = VT_I4;
		varSize.bVal = req->msg.data_len;
		hres = pInReq->lpVtbl->Put(pInReq, bstrReqDataSize, 0, &varSize, 0);
		if (FAILED(hres))
		{
			XPUM_LOG_WARN("Put RequestDataSize Failure: %d\n", hres);
			break;
		}

		SAFEARRAYBOUND saBound;
		saBound.lLbound = 0;
		saBound.cElements = req->msg.data_len;
		psa = SafeArrayCreate(VT_UI1, 1, &saBound);
		if (!psa)
		{
            XPUM_LOG_WARN("SafeArrayCreate Failure: %d\n", hres);
			break;
		}

		memcpy(psa->pvData, req->msg.data, req->msg.data_len);

		handle_variant(&varData);
		varData.vt = VT_ARRAY | VT_UI1;
		varData.parray = psa;
		hres = pInReq->lpVtbl->Put(pInReq, bstrReqData, 0, &varData, 0);
		if (FAILED(hres))
		{
			XPUM_LOG_WARN("Put RequestData Failure: %d\n", hres);
			break;
		}

		hres = pSvc->lpVtbl->ExecMethod(pSvc, V_BSTR(&varPath), bstrReqResp, 0, NULL,
			pInReq, &pOutRecv, NULL);
		if (FAILED(hres))
		{
			XPUM_LOG_WARN("ExecMethod RequestResponse Failure: %d\n", hres);
		}
		else
		{
			VARIANT varByte, varRespSize, varRespData;
			long respLen;

			handle_variant(&varByte);
			handle_variant(&varRespSize);
			handle_variant(&varRespData);

			hres = pOutRecv->lpVtbl->Get(pOutRecv, bstrCompCode, 0, &varByte, NULL, 0);
			if (FAILED(hres))
			{
				XPUM_LOG_WARN("Get CompletionCode Failure: %d\n", hres);
				break;
			}

			res->msg.data[0] = V_UI1(&varByte);

			hres = pOutRecv->lpVtbl->Get(pOutRecv, bstrRespDataSize, 0, &varRespSize, NULL, 0);
			if (FAILED(hres))
			{
				XPUM_LOG_WARN("Get ResponseDataSize Failure: %d\n", hres);
				break;
			}

			respLen = V_I4(&varRespSize);
			if (respLen > 1)
			{
				respLen--;
			}

			if (respLen > res->msg.data_len)
			{
				respLen = res->msg.data_len;
			}

			res->msg.data_len = (int)respLen;

			hres = pOutRecv->lpVtbl->Get(pOutRecv, bstrRespData, 0, &varRespData, NULL, 0);
			if (FAILED(hres))
			{
				XPUM_LOG_WARN("Get ResponseData Failure: %d\n", hres);
			}
			else
			{
				pRespData = (uchar*)varRespData.parray->pvData;
				for (int i = 0; i <= respLen; i++)
				{
					if (i > 0)
					{
						res->msg.data[i - 1] = pRespData[i];
					}
				}

				retval = 0;
			}
            memset(ss, 0, 1023);
            for (int i = 0; i < res->msg.data_len; ++i) {
                    snprintf(ss + i * 3, 1022,"%02x ", (int)res->msg.data[i]);
            }
            XPUM_LOG_INFO("ipmi_cmd_win res %d || %s\n", res->msg.data_len, ss);
		}
	} while (n);

	/*if (psa != NULL)
	{
		SafeArrayDestroy(psa);
		psa = NULL;
	}*/

	if (pInReq != NULL)
	{
		pInReq->lpVtbl->Release(pInReq);
		pInReq = NULL;
	}

	if (pOutRecv != NULL)
	{
		pOutRecv->lpVtbl->Release(pOutRecv);
		pOutRecv = NULL;
	}

	for (int i = 0; i < MAX_VARIANT_LIST_SIZE; i++)
	{
		VariantClear(variantList[i]);
	}
    varListIndex = 0;

	SysFreeString(bstrCommand);
	SysFreeString(bstrNetfn);
	SysFreeString(bstrLun);
	SysFreeString(bstrRespAddr);
	SysFreeString(bstrReqDataSize);
	SysFreeString(bstrReqData);
	SysFreeString(bstrReqResp);
	SysFreeString(bstrCompCode);
	SysFreeString(bstrRespDataSize);
	SysFreeString(bstrRespData);

	return retval;
}

void handle_variant(VARIANT *var)
{
	VariantInit(var);
	variantList[varListIndex++] = var;
}

#endif /* _WIN32 */

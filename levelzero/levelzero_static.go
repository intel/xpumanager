// Copyright (C) 2019-2025 Intel Corporation
//
// SPDX-License-Identifier: MIT
//

//go:generate go tool stringer -type ZeResult -output zz_strings.go -trimprefix ZE_RESULT_

package levelzero

import (
	"errors"
)

func boolToByte(b bool) byte {
	if b {
		return 1
	}
	return 0
}

func (r *ZeResult) ToError() error {
	if *r == ZE_RESULT_SUCCESS {
		return nil
	}
	return errors.New(r.String())
}

func ZesInit(flags ZesInitFlags) error {
	ret := zesInit(flags)
	return ret.ToError()
}

func ZesDriverGet() ([]*ZeDriver, error) {
	count := uint32(0)
	if ret := zesDriverGet(&count, nil); ret != ZE_RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	handles := make([]zeDriverHandle, count)
	ret := zesDriverGet(&count, handles)
	return handlesToWrappers[zeDriverHandle, ZeDriver](handles), ret.ToError()
}

func (z *ZeDriver) GetExtensionProperties() ([]ZesDriverExtensionProperties, error) {
	count := uint32(0)
	if ret := zesDriverGetExtensionProperties(z.handle, &count, nil); ret != ZE_RESULT_SUCCESS {
		return nil, ret.ToError()
	}
	extensions := make([]ZesDriverExtensionProperties, count)
	ret := zesDriverGetExtensionProperties(z.handle, &count, extensions)
	return extensions, ret.ToError()
}

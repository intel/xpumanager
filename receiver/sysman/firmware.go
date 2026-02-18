//
// Copyright (C) 2026 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0

package sysman

import (
	"fmt"
	"strings"

	"go.uber.org/zap"

	l0sysman "github.com/intel/level-zero-go/sysman"
)

// NOTE: we don't register firmware as a subsystem (registerSubsystem())
// because we don't expose any per-firmware metrics.

type firmware struct {
	*l0sysman.Firmware
	attributes firmwareAttributes
}

type firmwareAttributes struct {
	firmwareName    string
	firmwareVersion string
	subdeviceId     string
}

func enumFirmwares(d *device) []*firmware {
	fws, err := d.EnumFirmwares()
	if err != nil {
		d.logger.Errorw("Failed to enumerate firmwares", zap.Error(err))
		return nil
	}
	ret := make([]*firmware, 0, len(fws))
	for _, fw := range fws {
		m, err := newFirmware(fw)
		if err != nil {
			d.logger.Errorw("Failed to create Sysman firmware", zap.Error(err))
			continue
		}
		ret = append(ret, m)
	}
	return ret
}

func newFirmware(fw *l0sysman.Firmware) (*firmware, error) {
	props, err := fw.GetProperties()
	if err != nil {
		return nil, fmt.Errorf("firmware GetProperties() failed: %w", err)
	}

	return &firmware{
		Firmware: fw,
		attributes: firmwareAttributes{
			firmwareName:    strings.ToLower(props.Name.String()),
			firmwareVersion: props.Version.String(),
			subdeviceId:     subDeviceIdString(props.OnSubdevice, props.SubdeviceId),
		},
	}, nil
}

#
# Copyright (C) 2021-2022 Intel Corporation
# SPDX-License-Identifier: MIT
# @file dev_file_converter.py
#

import re
import os
import glob

dev_file_2_bdf_map = {}
bdf_2_dev_file_map = {}

pci_slot_name_pattern = re.compile(
    '^PCI_SLOT_NAME=([0-9a-fA-F]{4}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}\.[0-9a-fA-F]{1}\S*)')

dev_file_pattern = re.compile('(card\d+)\-\d*')

for dev_file in glob.glob("/dev/dri/card*"):
    dev_file_basename = os.path.basename(dev_file)
    with open(f'/sys/class/drm/{dev_file_basename}/device/uevent', 'r') as f:
        for line in f.readlines():
            line = line.strip()
            match = pci_slot_name_pattern.match(line)
            if match is not None:
                bdf_2_dev_file_map[match.group(1)] = dev_file_basename
                dev_file_2_bdf_map[dev_file_basename] = match.group(1)


def get_bdf_address(dev_file):
    device_id_match = dev_file_pattern.fullmatch(dev_file)
    if device_id_match is not None:
        return dev_file_2_bdf_map.get(device_id_match.group(1))

    return None


def get_dev_file(bdf_addr):
    return bdf_2_dev_file_map.get(bdf_addr)


if __name__ == '__main__':
    print(get_dev_file('0000:02:00.0'))
    print(get_bdf_address('card1-0'))

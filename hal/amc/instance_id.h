/*
 * Copyright (C) 2026 Intel Corporation
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef __INSTANCE_ID_H
#define __INSTANCE_ID_H

#include <cstdint>

/**
 * @brief Instance IDs for the requests of one message class (DSP0236 MCTP control, DSP0240 PLDM)
 *
 * The Instance ID is 5 bits. Every new request takes the next one; a retry of a request keeps the ID it
 * was first sent with, so the responder can tell a retry from a new request. Taking the ID once per
 * request, whatever its outcome, means a request that got no response never hands its ID to the next,
 * different request.
 */
class InstanceIdPool
{
public:
	/**
	 * @brief The Instance ID for a new request
	 * @return uint8_t 0 to 31, cycling
	 */
	uint8_t next()
	{
		const uint8_t id = mNext;
		mNext = static_cast<uint8_t>((mNext + 1) & INSTANCE_ID_MASK);
		return id;
	}

private:
	static constexpr uint8_t INSTANCE_ID_MASK = 0x1F;
	uint8_t mNext = 0;
};

#endif // __INSTANCE_ID_H

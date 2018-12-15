/**-------------------------------------------------------------------------
@file	agm_icm20948.cpp

@brief	Implementation of TDK ICM-20948 accel, gyro, mag sensor

@author	Hoang Nguyen Hoan
@date	Nov. 5, 2018

@license

Copyright (c) 2018, I-SYST inc., all rights reserved

Permission to use, copy, modify, and distribute this software for any purpose
with or without fee is hereby granted, provided that the above copyright
notice and this permission notice appear in all copies, and none of the
names : I-SYST or its contributors may be used to endorse or
promote products derived from this software without specific prior written
permission.

For info or contributing contact : hnhoan at i-syst dot com

THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

----------------------------------------------------------------------------*/

#include "idelay.h"
#include "coredev/i2c.h"
#include "coredev/spi.h"
#include "sensors/agm_icm20948.h"

bool AgmIcm20948::Init(uint32_t DevAddr, DeviceIntrf *pIntrf, Timer *pTimer)
{
	if (vbInitialized)
		return true;;

	if (pIntrf == NULL)
		return false;

	uint8_t regaddr;
	uint8_t d;
	uint8_t userctrl = 0;///*MPU9250_AG_USER_CTRL_FIFO_EN | MPU9250_AG_USER_CTRL_DMP_EN |*/ MPU9250_AG_USER_CTRL_I2C_MST_EN;
	uint8_t mst = 0;

	Interface(pIntrf);
	DeviceAddess(DevAddr);

	if (pTimer != NULL)
	{
		vpTimer = pTimer;
	}

	if (DevAddr == ICM20948_I2C_DEV_ADDR0 || DevAddr == ICM20948_I2C_DEV_ADDR1)
	{
		// I2C mode
		vbSpi = false;
	}
	else
	{
		vbSpi = true;

		// in SPI mode, use i2c master mode to access Mag device (AK8963C)
		//userctrl |= MPU9250_AG_USER_CTRL_I2C_MST_EN | MPU9250_AG_USER_CTRL_I2C_IF_DIS;
		//mst = MPU9250_AG_I2C_MST_CTRL_WAIT_FOR_ES | 13;
	}

	// Read chip id
	regaddr = ICM20948_WHO_AM_I;
	d = Read8((uint8_t*)&regaddr, 1);

	if (d != ICM20948_WHO_AM_I_ID)
	{
		return false;
	}

	Reset();

	DeviceID(d);
	Valid(true);


	// NOTE : require delay for reset to stabilize
	// the chip would not respond properly to motion detection
	usDelay(500000);

	regaddr = ICM20948_PWR_MGMT_1;
	Write8(&regaddr, 1, ICM20948_PWR_MGMT_1_SLEEP | 1);

	return true;
}

bool AgmIcm20948::Init(const ACCELSENSOR_CFG &CfgData, DeviceIntrf *pIntrf, Timer *pTimer)
{
	uint8_t regaddr;
	uint8_t d;

	if (Init(CfgData.DevAddr, pIntrf, pTimer) == false)
		return false;

	//regaddr = MPU9250_AG_LP_ACCEL_ODR;

	if (CfgData.Freq < 400)
	{
		Write8(&regaddr, 1, 0);
		vSampFreq = 240;	// 0.24 Hz
	}
	else if (CfgData.Freq < 900)
	{
		Write8(&regaddr, 1, 1);
		vSampFreq = 490;	// 0.49 Hz
	}
	else if (CfgData.Freq < 1500)
	{
		Write8(&regaddr, 1, 2);
		vSampFreq = 980;	// 0.98 Hz
	}
	else if (CfgData.Freq < 2500)
	{
		Write8(&regaddr, 1, 3);
		vSampFreq = 1950;	// 1.95 Hz
	}
	else if (CfgData.Freq < 3500)
	{
		Write8(&regaddr, 1, 4);
		vSampFreq = 3910;	// 3.91 Hz
	}
	else if (CfgData.Freq < 10000)
	{
		Write8(&regaddr, 1, 5);
		vSampFreq = 7810;	// 7.81 Hz
	}
	else if (CfgData.Freq < 20000)
	{
		Write8(&regaddr, 1, 6);
		vSampFreq = 15630;	// 15.63 Hz
	}
	else if (CfgData.Freq < 50000)
	{
		Write8(&regaddr, 1, 7);
		vSampFreq = 31250;	// 31.25 Hz
	}
	else if (CfgData.Freq < 100000)
	{
		Write8(&regaddr, 1, 8);
		vSampFreq = 62500;	// 62.5 Hz
	}
	else if (CfgData.Freq < 200000)
	{
		Write8(&regaddr, 1, 9);
		vSampFreq = 125000;	// 125 Hz
	}
	else if (CfgData.Freq < 500)
	{
		Write8(&regaddr, 1, 10);
		vSampFreq = 250000;	// 250 Hz
	}
	else
	{
		Write8(&regaddr, 1, 11);
		vSampFreq = 500000;	// 500 Hz
	}

	Scale(CfgData.Scale);
	LowPassFreq(vSampFreq / 2000);

	//regaddr = MPU9250_AG_INT_ENABLE;
	//Write8(&regaddr, 1, MPU9250_AG_INT_ENABLE_DMP_EN);

//	Reset();

	msDelay(100);

//	regaddr = MPU9250_AG_PWR_MGMT_1;
//	Write8(&regaddr, 1, MPU9250_AG_PWR_MGMT_1_CYCLE);

	return true;
}

bool AgmIcm20948::Init(const GYROSENSOR_CFG &CfgData, DeviceIntrf *pIntrf, Timer *pTimer)
{
	if (Init(CfgData.DevAddr, pIntrf, pTimer) == false)
		return false;

	uint8_t regaddr;
	uint8_t d = 0;
	uint8_t fchoice = 0;
	uint32_t f = CfgData.Freq >> 1;
#if 0
	if (f == 0)
	{
		fchoice = 1;
	}
	if (f < 10000)
	{
		d = MPU9250_AG_CONFIG_DLPF_CFG_5HZ;
	}
	else if (f < 20000)
	{
		d = MPU9250_AG_CONFIG_DLPF_CFG_10HZ;
	}
	else if (f < 30000)
	{
		d = MPU9250_AG_CONFIG_DLPF_CFG_20HZ;
	}
	else if (f < 60000)
	{
		d = MPU9250_AG_CONFIG_DLPF_CFG_41HZ;
	}
	else if (f < 150000)
	{
		d = MPU9250_AG_CONFIG_DLPF_CFG_92HZ;
	}
	else if (f < 220000)
	{
		d = MPU9250_AG_CONFIG_DLPF_CFG_184HZ;
	}
	else if (f < 40000)
	{
		d = MPU9250_AG_CONFIG_DLPF_CFG_250HZ;
	}
	else if (f < 400000)
	{
		d = MPU9250_AG_CONFIG_DLPF_CFG_3600HZ;
	}
	else
	{
		// 8800Hz
		fchoice = 1;
	}

	regaddr = MPU9250_AG_CONFIG;
	Write8(&regaddr, 1, d);

	regaddr = MPU9250_AG_GYRO_CONFIG;
	Write8(&regaddr, 1, fchoice);

#endif
	Sensitivity(CfgData.Sensitivity);

	return true;
}

bool AgmIcm20948::Init(const MAGSENSOR_CFG &CfgData, DeviceIntrf *pIntrf, Timer *pTimer)
{
	uint8_t regaddr;
	uint8_t d[4];

	if (Init(CfgData.DevAddr, pIntrf, pTimer) == false)
		return false;

	msDelay(200);
#if 0
	regaddr = MPU9250_MAG_WIA;
	Read(MPU9250_MAG_I2C_DEVADDR, &regaddr, 1, d, 1);

	if (d[0] != MPU9250_MAG_WIA_DEVICE_ID)
	{
		return false;
	}

	msDelay(1);

	// Read ROM sensitivity adjustment values
	regaddr = MPU9250_MAG_CTRL1;
	d[0] = MPU9250_MAG_CTRL1_MODE_PWRDOWN;
	Write(MPU9250_MAG_I2C_DEVADDR, &regaddr, 1, d, 1);

	msDelay(1);

	d[0] = MPU9250_MAG_CTRL1_MODE_FUSEROM_ACCESS;
	Write(MPU9250_MAG_I2C_DEVADDR, &regaddr, 1, d, 1);

	msDelay(100);

	regaddr = MPU9250_MAG_ASAX;
	Read(MPU9250_MAG_I2C_DEVADDR, &regaddr, 1, d, 3);

	vMagSenAdj[0] = (int16_t)d[0] - 128;
	vMagSenAdj[1] = (int16_t)d[1] - 128;
	vMagSenAdj[2] = (int16_t)d[2] - 128;

	// Transition out of reading ROM
	regaddr = MPU9250_MAG_CTRL1;
	d[0] = MPU9250_MAG_CTRL1_MODE_PWRDOWN;
	Write(MPU9250_MAG_I2C_DEVADDR, &regaddr, 1, d, 1);

	MagSensor::vPrecision = 14;
	vMagCtrl1Val = 0;
	MagSensor::vScale = 8190;

	if (CfgData.Precision >= 16)
	{
		MagSensor::vPrecision = 16;
		MagSensor::vScale = 32760;
		vMagCtrl1Val = MPU9250_MAG_CTRL1_BIT_16;
	}

	if (CfgData.OpMode == SENSOR_OPMODE_CONTINUOUS)
	{
		if (CfgData.Freq < 50000)
		{
			// Select 8Hz
			vMagCtrl1Val |= MPU9250_MAG_CTRL1_MODE_8HZ;
			MagSensor::Mode(CfgData.OpMode, 8000000);
		}
		else
		{
			// Select 100Hz
			vMagCtrl1Val |= MPU9250_MAG_CTRL1_MODE_100HZ;
			MagSensor::Mode(CfgData.OpMode, 100000000);
		}
	}
	else
	{
		vMagCtrl1Val |= MPU9250_MAG_CTRL1_MODE_SINGLE;
		MagSensor::Mode(CfgData.OpMode, 0);
	}

	msDelay(10);

	regaddr = MPU9250_MAG_CTRL1;
	Write(MPU9250_MAG_I2C_DEVADDR, &regaddr, 1, &vMagCtrl1Val, 1);
#endif
	return true;
}

bool AgmIcm20948::Enable()
{
#if 0
	uint8_t regaddr = MPU9250_AG_PWR_MGMT_1;

	Write8(&regaddr, 1, MPU9250_AG_PWR_MGMT_1_CYCLE | MPU9250_AG_PWR_MGMT_1_GYRO_STANDBY |
			MPU9250_AG_PWR_MGMT_1_CLKSEL_INTERNAL);

	regaddr = MPU9250_AG_PWR_MGMT_2;

	// Enable Accel & Gyro
	Write8(&regaddr, 1,
			MPU9250_AG_PWR_MGMT_2_DIS_ZG |
			MPU9250_AG_PWR_MGMT_2_DIS_YG |
			MPU9250_AG_PWR_MGMT_2_DIS_XG);

	// Enable Mag
	//regaddr = MPU9250_MAG_CTRL1;
	//Write(MPU9250_MAG_I2C_DEVADDR, &regaddr, 1, &vMagCtrl1Val, 1);
#endif
	return true;
}

void AgmIcm20948::Disable()
{
#if 0
	uint8_t regaddr = MPU9250_AG_PWR_MGMT_2;

Reset();
msDelay(2000);

	regaddr = MPU9250_AG_PWR_MGMT_1;
	Write8(&regaddr, 1, MPU9250_AG_PWR_MGMT_1_SLEEP | MPU9250_AG_PWR_MGMT_1_PD_PTAT |
						MPU9250_AG_PWR_MGMT_1_GYRO_STANDBY);

	//return;

	regaddr = MPU9250_AG_USER_CTRL;
	Write8(&regaddr, 1, MPU9250_AG_USER_CTRL_I2C_MST_EN);

	// Disable Mag
	regaddr = MPU9250_MAG_CTRL1;
	uint8_t d = 0;
	Write(MPU9250_MAG_I2C_DEVADDR, &regaddr, 1, &d, 1);

	// Disable Accel Gyro
	Write8(&regaddr, 1,
		 MPU9250_AG_PWR_MGMT_2_DIS_ZG | MPU9250_AG_PWR_MGMT_2_DIS_YG | MPU9250_AG_PWR_MGMT_2_DIS_XG |
		 MPU9250_AG_PWR_MGMT_2_DIS_ZA | MPU9250_AG_PWR_MGMT_2_DIS_YA | MPU9250_AG_PWR_MGMT_2_DIS_XA);

	regaddr = MPU9250_AG_PWR_MGMT_1;
	Write8(&regaddr, 1, MPU9250_AG_PWR_MGMT_1_SLEEP | MPU9250_AG_PWR_MGMT_1_PD_PTAT |
						MPU9250_AG_PWR_MGMT_1_GYRO_STANDBY);
#endif
}

void AgmIcm20948::Reset()
{
	uint8_t regaddr = ICM20948_PWR_MGMT_1;

	Write8(&regaddr, 1, ICM20948_PWR_MGMT_1_DEVICE_RESET);
}

bool AgmIcm20948::StartSampling()
{
	return true;
}

// Implement wake on motion
bool AgmIcm20948::WakeOnEvent(bool bEnable, int Threshold)
{
    uint8_t regaddr;

	if (bEnable == true)
	{
		Reset();

		msDelay(2000);
	}
	else
	{
//	    regaddr = MPU9250_AG_INT_ENABLE;
	    Write8(&regaddr, 1, 0);

//	    regaddr = MPU9250_AG_PWR_MGMT_1;
		Write8(&regaddr, 1, 0);
	}

	return true;
}

// Accel low pass frequency
uint32_t AgmIcm20948::LowPassFreq(uint32_t Freq)
{
	return AccelSensor::LowPassFreq();
}

// Accel scale
uint16_t AgmIcm20948::Scale(uint16_t Value)
{
	return AccelSensor::Scale();
}

// Gyro scale
uint32_t AgmIcm20948::Sensitivity(uint32_t Value)
{

	return GyroSensor::Sensitivity();
}

bool AgmIcm20948::UpdateData()
{
#if 0
	uint8_t regaddr = MPU9250_AG_FIFO_COUNT_H;//MPU9250_AG_ACCEL_XOUT_H;
	int8_t d[20];
	int32_t val;

	Read(&regaddr, 1, (uint8_t*)d, 2);
	val = ((d[0] & 0xF) << 8) | d[1];

	//printf("%d\r\n", val);

	if (val > 0)
	{
		int cnt = min(val, 18);
		regaddr = MPU9250_AG_FIFO_R_W;
	//	Read(&regaddr, 1, d, cnt);
	}

	vSampleCnt++;

	if (vpTimer)
	{
		vSampleTime = vpTimer->uSecond();
	}

	regaddr = MPU9250_AG_ACCEL_XOUT_H;
	Read(&regaddr, 1, (uint8_t*)d, 6);

	int32_t scale =  AccelSensor::Scale();
	val = (((((int32_t)d[0] << 8) | d[1]) * scale) << 8L) / 0x7FFF;
	AccelSensor::vData.X = val;
	val = (((((int32_t)d[2] << 8) | d[3]) * scale) << 8L) / 0x7FFF;
	AccelSensor::vData.Y = val;
	val = (((((int32_t)d[4] << 8) | d[5]) * scale) << 8L) / 0x7FFF;
	AccelSensor::vData.Z = val;
	AccelSensor::vData.Timestamp = vSampleTime;

	regaddr = MPU9250_AG_GYRO_XOUT_H;

	Read(&regaddr, 1, (uint8_t*)d, 6);

	val = ((((int16_t)d[0] << 8) | d[1]) << 8) / GyroSensor::vSensitivity;
	GyroSensor::vData.X = val;
	val = ((((int16_t)d[2] << 8) | d[3]) << 8) / GyroSensor::vSensitivity;
	GyroSensor::vData.Y = val;
	val = ((((int32_t)d[4] << 8) | d[5]) << 8L) / GyroSensor::vSensitivity;
	GyroSensor::vData.Z = val;
	GyroSensor::vData.Timestamp = vSampleTime;

	regaddr = MPU9250_MAG_ST1;
	Read(MPU9250_MAG_I2C_DEVADDR, &regaddr, 1, (uint8_t*)d, 8);

	if (d[14] & MPU9250_MAG_ST1_DRDY)
	{
		val = (((int16_t)d[0]) << 8L) | d[1];
		val += (val * vMagSenAdj[0]) >> 8L;
		MagSensor::vData.X = (int16_t)(val * (MPU9250_MAG_MAX_FLUX_DENSITY << 8) / MagSensor::vScale);

		val = (((int16_t)d[2]) << 8) | d[3];
		val += (val * vMagSenAdj[1]) >> 8L;
		MagSensor::vData.Y = (int16_t)(val * (MPU9250_MAG_MAX_FLUX_DENSITY << 8) / MagSensor::vScale);

		val = (((int16_t)d[4]) << 8) | d[5];
		val += (val * vMagSenAdj[2]) >> 8L;
		MagSensor::vData.Z = (int16_t)(val * (MPU9250_MAG_MAX_FLUX_DENSITY << 8) / MagSensor::vScale);

		MagSensor::vData.Timestamp = vSampleTime;
	}
#endif
	return true;
}

bool AgmIcm20948::Read(ACCELSENSOR_DATA &Data)
{
	Data = AccelSensor::vData;

	return true;
}

bool AgmIcm20948::Read(GYROSENSOR_DATA &Data)
{
	Data = GyroSensor::vData;

	return true;
}

bool AgmIcm20948::Read(MAGSENSOR_DATA &Data)
{
	Data = MagSensor::vData;

	return true;
}

int AgmIcm20948::Read(uint8_t *pCmdAddr, int CmdAddrLen, uint8_t *pBuff, int BuffLen)
{
	if (vbSpi == true)
	{
		*pCmdAddr |= 0x80;
	}

	return Device::Read(pCmdAddr, CmdAddrLen, pBuff, BuffLen);
}


int AgmIcm20948::Write(uint8_t *pCmdAddr, int CmdAddrLen, uint8_t *pData, int DataLen)
{
	if (vbSpi == true)
	{
		*pCmdAddr &= 0x7F;
	}

	return Device::Write(pCmdAddr, CmdAddrLen, pData, DataLen);
}

int AgmIcm20948::Read(uint8_t DevAddr, uint8_t *pCmdAddr, int CmdAddrLen, uint8_t *pBuff, int BuffLen)
{
	int retval = 0;

	if (vbSpi)
	{
		uint8_t regaddr;
		uint8_t d[8];
#if 0
		d[0] = MPU9250_AG_I2C_SLV0_ADDR;
		d[1] = DevAddr | MPU9250_AG_I2C_SLV0_ADDR_I2C_SLVO_RD;
		d[2] = *pCmdAddr;

		while (BuffLen > 0)
		{
			int cnt = min(15, BuffLen);

			d[3] = MPU9250_AG_I2C_SLV0_CTRL_I2C_SLV0_EN |cnt;

			Write(d, 4, NULL, 0);

			// Delay require for transfer to complete
			//usDelay(500 + (cnt << 4));
			msDelay(1);

			regaddr = MPU9250_AG_EXT_SENS_DATA_00;

			cnt = Read(&regaddr, 1, pBuff, cnt);
			if (cnt <=0)
				break;

			pBuff += cnt;
			BuffLen -= cnt;
			retval += cnt;
		}
#endif
	}
	else
	{
		retval = vpIntrf->Read(DevAddr, pCmdAddr, CmdAddrLen, pBuff, BuffLen);
	}

	return retval;
}

int AgmIcm20948::Write(uint8_t DevAddr, uint8_t *pCmdAddr, int CmdAddrLen, uint8_t *pData, int DataLen)
{
	int retval = 0;

	if (vbSpi)
	{
		uint8_t regaddr;
		uint8_t d[8];
#if 0
		d[0] = MPU9250_AG_I2C_SLV0_ADDR;
		d[1] = DevAddr;
		d[2] = *pCmdAddr;
		d[3] = MPU9250_AG_I2C_SLV0_CTRL_I2C_SLV0_EN;

		while (DataLen > 0)
		{
			regaddr = MPU9250_AG_I2C_SLV0_DO;
			Write8(&regaddr, 1, *pData);

			Write(d, 4, NULL, 0);

			d[2]++;
			pData++;
			DataLen--;
			retval++;
		}
#endif
	}
	else
	{
		retval = vpIntrf->Write(DevAddr, pCmdAddr, CmdAddrLen, pData, DataLen);
	}

	return retval;
}

void AgmIcm20948::IntHandler()
{
	uint8_t regaddr = 0;//MPU9250_AG_INT_STATUS;
	uint8_t d;

	d = Read8(&regaddr, 1);
//	if (d & MPU9250_AG_INT_STATUS_RAW_DATA_RDY_INT)
	{
		UpdateData();
	}
}


/*!
 * @file readOzoneData.ino
 * @brief Reading ozone concentration, A concentration of one part per billion (PPB).
 * @n step: we must first determine the iic device address, will dial the code switch A0, A1 (OZONE_ADDRESS_0 for [0 0]), (OZONE_ADDRESS_1 for [1 0]), (OZONE_ADDRESS_2 for [0 1]), (OZONE_ADDRESS_3 for [1 1]).
 * @n       Then configure the mode of active and passive acquisition, Finally, ozone data can be read.
 * @n note: it takes time to stable oxygen concentration, about 3 minutes.
 * @copyright Copyright (c) 2010 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license The MIT License (MIT)
 * @author [ZhixinLiu](zhixin.liu@dfrobot.com)
 * @version V1.0
 * @date 2019-10-10
 * @url https://github.com/DFRobot/DFRobot_OzoneSensor
 */
#include "DFRobot_OzoneSensor.h"

#define COLLECT_NUMBER  20              // collect number, the collection range is 1-100
/**
 * select i2c device address 
 *   OZONE_ADDRESS_0  0x70
 *   OZONE_ADDRESS_1  0x71
 *   OZONE_ADDRESS_2  0x72
 *   OZONE_ADDRESS_3  0x73
 */
#define Ozone_IICAddress OZONE_ADDRESS_3
DFRobot_OzoneSensor Ozone;

void setup() 
{
  Serial.begin(9600);
  while(!Ozone.begin(Ozone_IICAddress)){
    Serial.println("I2c device number error !");
    delay(1000);
  }
  Serial.println("I2c connect success !");

  /**
   * set measuer mode
   * MEASURE_MODE_AUTOMATIC         active  mode
   * MEASURE_MODE_PASSIVE           passive mode
   */
    Ozone.setModes(MEASURE_MODE_PASSIVE);
}


void loop() 
{
  /**
   * Smooth data collection
   * COLLECT_NUMBER                 The collection range is 1-100
   */
  int16_t ozoneConcentration = Ozone.readOzoneData(COLLECT_NUMBER);
  Serial.print("Ozone concentration is ");
  Serial.print(ozoneConcentration);
  Serial.println(" PPB.");
  delay(1000);
}
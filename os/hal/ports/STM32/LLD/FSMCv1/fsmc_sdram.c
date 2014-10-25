/*
    ChibiOS/HAL - Copyright (C) 2006-2014 Giovanni Di Sirio

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/
/*
   Concepts and parts of this file have been contributed by Uladzimir Pylinsky
   aka barthess (SDRAM routines added by Nick Klimov aka progfin).
 */

/**
 * @file    fsmc_sdram.c
 * @brief   SDRAM Driver subsystem low level driver source.
 *
 * @addtogroup SDRAM
 * @{
 */

#include "hal.h"

#if (defined(STM32F427xx) || defined(STM32F437xx) || \
     defined(STM32F429xx) || defined(STM32F439xx))

#if STM32_USE_FSMC_SDRAM || defined(__DOXYGEN__)

#include "fsmc_sdram.h"

/*===========================================================================*/
/* Driver local definitions.                                                 */
/*===========================================================================*/

/**
 * FMC_Command_Mode
 */
#define FMC_Command_Mode_normal            ((uint32_t)0x00000000)
#define FMC_Command_Mode_CLK_Enabled       ((uint32_t)0x00000001)
#define FMC_Command_Mode_PALL              ((uint32_t)0x00000002)
#define FMC_Command_Mode_AutoRefresh       ((uint32_t)0x00000003)
#define FMC_Command_Mode_LoadMode          ((uint32_t)0x00000004)
#define FMC_Command_Mode_Selfrefresh       ((uint32_t)0x00000005)
#define FMC_Command_Mode_PowerDown         ((uint32_t)0x00000006)

/*===========================================================================*/
/* Driver exported variables.                                                */
/*===========================================================================*/
/**
 * @brief   SDRAM driver identifier.
 */
SDRAMDriver SDRAMD;

/*===========================================================================*/
/* Driver local types.                                                       */
/*===========================================================================*/

/*===========================================================================*/
/* Driver local variables and types.                                         */
/*===========================================================================*/

/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/

/**
 * @brief   Wait until the SDRAM controller is ready.
 *
 * @notapi
 */
static void _sdram_wait_ready(void) {
  /* Wait until the SDRAM controller is ready */
  while (SDRAMD.sdram->SDSR & FMC_SDSR_BUSY);
}

/**
 * @brief   Executes the SDRAM memory initialization sequence.
 *
 * @param[in] cfgp         pointer to the @p SDRAMConfig object
 *
 * @notapi
 */
static void _sdram_init_sequence(const SDRAMConfig *cfgp) {

  uint32_t command_target = 0;

#if STM32_SDRAM_USE_FSMC_SDRAM1
  command_target |= FMC_SDCMR_CTB1;
#endif
#if STM32_SDRAM_USE_FSMC_SDRAM2
  command_target |= FMC_SDCMR_CTB2;
#endif

  /* Step 3: Configure a clock configuration enable command.*/
  _sdram_wait_ready();
  SDRAMD.sdram->SDCMR = FMC_Command_Mode_CLK_Enabled | command_target;

  /* Step 4: Insert 1 ms delay (tipically 100uS).*/
  osalThreadSleepMilliseconds(1);

  /* Step 5: Configure a PALL (precharge all) command.*/
  _sdram_wait_ready();
  SDRAMD.sdram->SDCMR = FMC_Command_Mode_PALL | command_target;

  /* Step 6.1: Configure a Auto-Refresh command: send the first command.*/
  _sdram_wait_ready();
  SDRAMD.sdram->SDCMR = FMC_Command_Mode_AutoRefresh | command_target |
      (cfgp->sdcmr & FMC_SDCMR_NRFS);

  /* Step 6.2: Send the second command.*/
  SDRAMD.sdram->SDCMR = FMC_Command_Mode_AutoRefresh | command_target |
      (cfgp->sdcmr & FMC_SDCMR_NRFS);

  /* Step 7: Program the external memory mode register.*/
  _sdram_wait_ready();
  SDRAMD.sdram->SDCMR = FMC_Command_Mode_LoadMode | command_target |
      (cfgp->sdcmr & FMC_SDCMR_MRD);

  /* Step 8: Set clock.*/
  _sdram_wait_ready();
  SDRAMD.sdram->SDRTR = cfgp->sdrtr;

  _sdram_wait_ready();
}

/*===========================================================================*/
/* Driver interrupt handlers.                                                */
/*===========================================================================*/

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Low level SDRAM driver initialization.
 */
void fsmcSdramInit(void) {

  fsmc_init();

  SDRAMD.sdram = FSMCD1.sdram;
  SDRAMD.state = SDRAM_STOP;
}

/**
 * @brief   Configures and activates the SDRAM peripheral.
 *
 * @param[in] sdramp        pointer to the @p SDRAMDriver object
 * @param[in] cfgp          pointer to the @p SDRAMConfig object
 */
void fsmcSdramStart(SDRAMDriver *sdramp, const SDRAMConfig *cfgp) {

  if (FSMCD1.state == FSMC_STOP)
    fsmc_start(&FSMCD1);

  osalDbgAssert((sdramp->state == SDRAM_STOP) || (sdramp->state == SDRAM_READY),
              "SDRAM. Invalid state.");

  if (sdramp->state == SDRAM_STOP) {

    /* Even if you need only bank2 you must properly set up SDCR and SDTR
       regitsters for bank1 too. Both banks will be tuned equally assuming
       connected memory ICs are equal.*/
    sdramp->sdram->banks[0].SDCR = cfgp->sdcr;
    sdramp->sdram->banks[0].SDTR = cfgp->sdtr;
    sdramp->sdram->banks[1].SDCR = cfgp->sdcr;
    sdramp->sdram->banks[1].SDTR = cfgp->sdtr;

    _sdram_init_sequence(cfgp);

    sdramp->state = SDRAM_READY;
  }
}

/**
 * @brief   Deactivates the SDRAM peripheral.
 *
 * @param[in] sdramp         pointer to the @p SDRAMDriver object
 *
 * @notapi
 */
void fsmcSdramStop(SDRAMDriver *sdramp) {

  if (sdramp->state == SDRAM_READY) {
    sdramp->state = SDRAM_STOP;
  }
}

#endif /* STM32_USE_FSMC_SDRAM */

#endif /* STM32F427xx / STM32F429xx / STM32F437xx / STM32F439xx */

/** @} */


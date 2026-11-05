#ifndef CONFIG_H_
#define CONFIG_H_

// Platform-side config for the SDL host build, copied from platforms/imx/config/config.h.
// CPU_FREQ and SPREAD_SPECTRUM are dropped; section placement has no meaning on a hosted OS.
// Core does not include this file: core/config/config.h is found through core's include path.

#define AUDIO_CORE_FLASH
#define AUDIO_CORE_RAM

static constexpr int kAudioBlockSamples = 128; // sync w config.inc

static constexpr int kFileBufferSize = 1024;
static constexpr int kFileOperationsExternalBufferSize = 1024 * 10;

static constexpr int kMaxPathLength = 255;

static constexpr char kConfigFilePath[] = "/trackerConfig";

static constexpr char kFirmwareInfoFilePath[] = "/Firmware/_fwinfo";
static constexpr char kTurnOnFilePath[] = "/Firmware/_turnon";

#ifdef FEATURE_SYNTH
// T32: mtStructs.h sizes the synth slots with core/config/config.h's constant, which this file shadows.
static constexpr int kSynthParts = 3;
#endif

#endif /* CONFIG_H_ */

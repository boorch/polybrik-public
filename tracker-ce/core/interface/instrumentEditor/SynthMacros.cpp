#include "instrumentEditor/instrumentEditor.h"
#if GUI_HANDLE_SYNTH

#include "core/interfacePopups.h"
#include "mtAudioEngine.h"

#include "PolySynth/Patch.h"

#include "cr_section_macros.h"
#include "fileManager.h"
#include "util_math.h"
#include "util_string.h"

static cInstrumentEditor* IE = &instrumentEditor;

static void functSynthMacroActionButtons(uint8_t button);
static void functSynthMacroEncoder(int16_t value);
static void functSynthMacroUp();
static void functSynthMacroDown();
static void functSynthMacroLeft();
static void functSynthMacroRight();
static void functDelete();
static void functSynthMacroMiniMiddle(uint8_t state);
static void functSynthMacroPads(uint8_t pad, uint8_t state, int16_t);

static void functSynthMacroShift(uint8_t state);

#ifdef PLATFORM_SDL
// Synth macros action list (core patch 34). Labels copy the firmware's "M<n> <name>" labels (set in
// showSynthMacrosNames), line 2 its value text (labelText, set in showSynthMacroValue).
#include "core/sdlActionList.h"

static char sdlMacroLabel[kSynthMacrosCount][11];
template <uint8_t P> static void sdlMacroNudge(int16_t d)
{
    IE->selectedPlace[mtInstEditModeSynthMacros] = P;
    IE->activateLabelsBorder();
    IE->changeSynthMacroParamValue(d);
}
template <uint8_t P> static void sdlMacroText(char* o, size_t n) { snprintf(o, n, "%s", IE->labelText[P]); }

#define SDL_VAL(id, label, nudge, text2) { id, label, SdlActionEntry::Value, nullptr, nudge, text2, nullptr }
static const SdlActionEntry kSynthMacrosEntries[] = {
    SDL_VAL("macro_1", sdlMacroLabel[0], sdlMacroNudge<0>, sdlMacroText<0>),
    SDL_VAL("macro_2", sdlMacroLabel[1], sdlMacroNudge<1>, sdlMacroText<1>),
    SDL_VAL("macro_3", sdlMacroLabel[2], sdlMacroNudge<2>, sdlMacroText<2>),
    SDL_VAL("macro_4", sdlMacroLabel[3], sdlMacroNudge<3>, sdlMacroText<3>),
    SDL_VAL("macro_5", sdlMacroLabel[4], sdlMacroNudge<4>, sdlMacroText<4>),
    SDL_VAL("macro_6", sdlMacroLabel[5], sdlMacroNudge<5>, sdlMacroText<5>),
};
#undef SDL_VAL
static_assert(kSynthMacrosCount == 6, "one entry per macro");
static const SdlActionPage kSynthMacrosPages[] = { { kSynthMacrosEntries, 6 } };
static const SdlActionListDef kSynthMacrosList = { "synth_macros", kSynthMacrosPages, 1 };
#endif

void cInstrumentEditor::setSynthMacrosScreenFunct()
{
    ip->clearButtonsRange(interfaceButton0, interfaceButton7);
    ip->clearAllPots();

    for (uint8_t i = interfaceButton0; i <= interfaceButton5; i++)
    {
        ip->setButtonObj(i, buttonPress, functSynthMacroActionButtons);
    }

    ip->setPotObj(interfacePot0, functSynthMacroEncoder);

    ip->setButtonObj(interfaceButtonLeft, buttonPress, functSynthMacroLeft);
    ip->setButtonObj(interfaceButtonRight, buttonPress, functSynthMacroRight);
    ip->setButtonObj(interfaceButtonUp, buttonPress, functSynthMacroUp);
    ip->setButtonObj(interfaceButtonDown, buttonPress, functSynthMacroDown);

    ip->setButtonObj(interfaceButtonDelete, buttonPress, functDelete);
    ip->setButtonObj(interfaceButtonMiniMiddle, functSynthMacroMiniMiddle);

    ip->setButtonObj(interfaceButtonShift, functSynthMacroShift);

    ip->setPadsGlobal(functSynthMacroPads);

    ip->clearButton(interfaceButtonInsert);
#ifdef PLATFORM_SDL
    sdlActionList.setList(&kSynthMacrosList);
#endif
}

void cInstrumentEditor::changeSynthMacroParamValue(int value)
{
    switch (selectedPlace[mtInstEditModeSynthMacros])
    {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5: changeSynthMacro(selectedPlace[mtInstEditModeSynthMacros], value); break;
        default: break;
    }
}

static void functSynthMacroActionButtons(uint8_t button)
{
    if (IE->mode != mtInstEditModeSynthMacros)
    {
        return;
    }

    IE->selectedPlace[IE->mode] = button;

    IE->activateLabelsBorder();
}

static void functSynthMacroEncoder(int16_t value)
{
    IE->changeSynthMacroParamValue(value);
}

static void functSynthMacroUp()
{
    IE->changeSynthMacroParamValue(1);
}

static void functSynthMacroDown()
{
    IE->changeSynthMacroParamValue(-1);
}

static void functSynthMacroLeft()
{
    if (IE->selectedPlace[mtInstEditModeSynthMacros] > 0)
    {
        IE->selectedPlace[mtInstEditModeSynthMacros]--;
    }

    IE->activateLabelsBorder();
}

static void functSynthMacroRight()
{
    if (IE->selectedPlace[mtInstEditModeSynthMacros] < IE->frameData.placesCount - 1)
    {
        IE->selectedPlace[mtInstEditModeSynthMacros]++;
    }

    IE->activateLabelsBorder();
}

static void functDelete()
{
    switch (IE->selectedPlace[mtInstEditModeSynthMacros])
    {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5: IE->setDefaultSynthMacroValue(IE->selectedPlace[mtInstEditModeSynthMacros]); break;
        default: break;
    }
}

static void functSynthMacroMiniMiddle(uint8_t state)
{
    IE->synthCommonPreviewButtonFunct(state);
}

static void functSynthMacroPads(uint8_t pad, uint8_t state, int16_t)
{
    IE->playSynthNoteByPad(pad, state);
}

static void functSynthMacroShift(uint8_t state)
{
    if (state == buttonPress)
    {
        if (mtPopups.isNoteKeyboardPopupActive())
        {
            mtPopups.toggleNoteKeyboardPopup(InterfacePopups::noteKeyboardOption::kShowEditButtons);
        }
    }
    else if (state == buttonRelease)
    {
        if (mtPopups.isNoteKeyboardPopupActive())
        {
            mtPopups.toggleNoteKeyboardPopup(InterfacePopups::noteKeyboardOption::kForceShow);
        }
    }
}

void cInstrumentEditor::changeSynthMacro(int macroIdx, int16_t delta)
{
    assert(macroIdx < kSynthMacrosCount);

    if (!isSynthInstrument(mtProject.values.lastUsedInstrument))
    {
        return;
    }

    int synthPartIdx = instrumentIdxToSynthIdx(mtProject.values.lastUsedInstrument);
    assert(!isOutOfRange(synthPartIdx, 0, kSynthParts - 1));

    int maxMacroValue = getMacroMax(synthPartIdx, macroIdx);
    SynthPartSettings& synthPart = mtProject.values.synthPart[synthPartIdx];

    synthPart.macroValue[macroIdx] =
      clamp<int>((int)synthPart.macroValue[macroIdx] + delta, kCCMinValue, maxMacroValue);

    setSynthMacro(synthPartIdx, macroIdx, (int)synthPart.macroValue[macroIdx]);
}

int cInstrumentEditor::getMacroMax(int synthPartIdx, int macroIdx)
{
    auto& patch = getSynth()->getPatch(synthPartIdx);
    const auto& metadata = patch.getMetadata();

    return metadata.macros[macroIdx].bipolar ? kCCMidValue * 2 : kCCMaxValue;
}

void cInstrumentEditor::setSynthMacro(int synthPartIdx, int macroIdx, int value, bool show)
{
    audioEngine::SynthParam synthParamIdx =
      audioEngine::SynthParam(audioEngine::SynthParam::kMacro1 + macroIdx);

    engine.setSynthParam(
      synthParamIdx, synthPartIdx, value, audioEngine::SynthValuesSource::kInstrument);

    newFileManager.setProjectStructChanged();

    if (show)
    {
        showSynthMacroValue(synthPartIdx, macroIdx);
    }
}

void cInstrumentEditor::cleanMacrosResources()
{
    if (macrosNameBufor != nullptr)
    {
        delete[] macrosNameBufor;
        macrosNameBufor = nullptr;
    }
}

void cInstrumentEditor::setDefaultSynthMacroValue(int macroIdx)
{
    assert(macroIdx < kSynthMacrosCount);

    if (!isSynthInstrument(mtProject.values.lastUsedInstrument))
    {
        return;
    }

    int synthPartIdx = instrumentIdxToSynthIdx(mtProject.values.lastUsedInstrument);
    assert(!isOutOfRange(synthPartIdx, 0, kSynthParts - 1));

    SynthPartSettings& synthPart = mtProject.values.synthPart[synthPartIdx];

    synthPart.macroValue[macroIdx] =
      isMacroBipolar(synthPartIdx, macroIdx) ? kCCMidValue : kCCMinValue;

    setSynthMacro(synthPartIdx, macroIdx, (int)synthPart.macroValue[macroIdx]);
}

// ------------------------------------------------------------------------------------------
// ------------------------------       GUI     ---------------------------------------------
// ------------------------------------------------------------------------------------------

void cInstrumentEditor::showSynthMacrosScreen()
{
    assert(mtProject.values.lastUsedInstrument >= SYNTH_INSTRUMENTS_START_IDX);

    showTitleBar();
    refreshFramePosition();

    for (uint8_t i = 0; i < 8; i++)
    {
        display.setControlColors(label[i], display.getTheme().activeLabelsColors);
        display.setControlColors(barControl[i], localActiveBarColors);
        display.setAddControlStyle(label[i], controlStyleLimitTextWidth);
    }
    display.setRemoveControlStyle(label[6], controlStyleShowBitmap);
    display.setRemoveControlStyle(label[6], controlStyleSpecialFx2);
    display.setRemoveControlStyle(label[7], controlStyleSpecialFx2);

    showSynthMacrosNames();

    display.setControlText(label[6], "");
    display.setControlText(label[7], "");
    display.setControlText2(label[0], "");
    display.setControlText2(label[1], "");
    display.setControlText2(label[2], "");
    display.setControlText2(label[3], "");
    display.setControlText2(label[4], "");
    display.setControlText2(label[5], "");
    display.setControlText2(label[6], "");
    display.setControlText2(label[7], "");
    display.setControlPosition(label[4], (800 / 8) * 4 + (800 / 16), -1);

    uint8_t syntPartIdx = mtProject.values.lastUsedInstrument - SYNTH_INSTRUMENTS_START_IDX;
    for (int macroIdx = 0; macroIdx < 6; macroIdx++)
    {
        showSynthMacroValue(syntPartIdx, macroIdx);
    }

    display.setControlHide(doubleLabelAutomation);
    display.setControlHide(instrumentSynthCCListControl);
    display.setControlHide(instrumentListControl);
    display.setControlHide(filterModeListControl);
    display.setControlHide(blankSpaceCover[0]);
    display.setControlHide(list1Control);
    display.setControlHide(list2Control);
    display.setControlHide(list3Control);
    display.setControlHide(list4Control);
    display.setControlHide(noteLabel);
    display.setControlHide(blankSpaceCover[0]);
    display.setControlHide(paramsListControl);
    display.setControlHide(modelListControl);
    display.setControlHide(imgGridControl);

    uint32_t shadow = (display.getTheme().bottomScreenShadow > 0 ? controlStyleBottomShadow : 0);
    uint32_t gradient = (display.getTheme().barShadow > 0 ? controlStyleGradient : 0);

    for (uint8_t i = 0; i < 8; i++)
    {
        display.setControlValue(label[i], 1);
        display.setControlStyle2(label[i],
                                 controlStyleCenterX | controlStyleCenterY | controlStyleFont2);
        display.setControlShow(label[i]);
        display.refreshControl(label[i]);

        uint32_t barStyle = controlStyleShow | controlStyleBackground;

        if (isMacroBipolar(syntPartIdx, i))
        {
            barStyle |= controlStyleValueLeftRight_100_100;
        }
        else
        {
            barStyle |= controlStyleValue_0_100;
        }

        display.setControlStyle(barControl[i], barStyle | shadow | gradient);
        display.setControlShow(barControl[i]);
        display.refreshControl(barControl[i]);
    }

    display.setControlHide(barControl[6]);
    display.setControlHide(barControl[7]);

    display.setControlValue(bgLabel, 255);
    display.refreshControl(bgLabel);
    display.setControlShow(bgLabel);

    display.setControlPosition(blankSpaceCover[0], (800 / 8) * 6 + 1, -1);
    display.setControlSize(blankSpaceCover[0], (800 / 8) * 2 - 3, 394);
    display.setControlShow(blankSpaceCover[0]);
    display.refreshControl(blankSpaceCover[0]);

    display.synchronizeRefresh();
}

void cInstrumentEditor::showSynthMacrosNames()
{
    int syntPartIdx = mtProject.values.lastUsedInstrument - SYNTH_INSTRUMENTS_START_IDX;

    auto& patch = getSynth()->getPatch(syntPartIdx);
    const auto& metadata = patch.getMetadata();

    if (macrosNameBufor != nullptr)
    {
        delete[] macrosNameBufor;
    }

    macrosNameBufor = new char[kSynthMacrosCount * kMaxSynthListStringLen];
#ifdef PLATFORM_SDL
    memset(sdlMacroLabel, 0, sizeof(sdlMacroLabel));
#endif

    int macroId = 0;
    int idx = 0;
    for (const auto& macro : metadata.macros)
    {
        if (macroId != *polysynth::Patch::SystemMacroId::kPressure)
        {
            snprintf(&macrosNameBufor[idx * kMaxSynthListStringLen],
                     kMaxSynthListStringLen,
                     "M%d %s",
                     macroId + 1,
                     macro.displayName.c_str());

            display.setControlText(label[idx], &macrosNameBufor[idx * kMaxSynthListStringLen]);
#ifdef PLATFORM_SDL
            snprintf(sdlMacroLabel[idx], sizeof(sdlMacroLabel[idx]), "%s", &macrosNameBufor[idx * kMaxSynthListStringLen]);
#endif

            idx++;
            if (idx >= kSynthMacrosCount)
            {
                break;
            }
        }
        macroId++;
    }
}

void cInstrumentEditor::showSynthMacroValue(uint8_t syntPartIdx, uint8_t macroIdx)
{
    assert(macroIdx < kSynthMacrosCount);
    assert(syntPartIdx < kSynthInstrumentsCount);

    int value = mtProject.values.synthPart[syntPartIdx].macroValue[macroIdx];
    int columnIdx = macroIdx;
    bool isBipolar = isMacroBipolar(syntPartIdx, macroIdx);

    snprintf(labelText[columnIdx],
             sizeof(labelText[columnIdx]),
             "%d ",
             isBipolar ? mapClamped(value, 0, getMacroMax(syntPartIdx, macroIdx), -63, 63) : value);

    display.setControlText2(label[columnIdx], labelText[columnIdx]);
    display.refreshControl(label[columnIdx]);

    int barValue = isBipolar
                     ? mapClamped(value, kCCMinValue, getMacroMax(syntPartIdx, macroIdx), -100, 100)
                     : mapClamped(value, kCCMinValue, getMacroMax(syntPartIdx, macroIdx), 0, 100);

    display.setControlValue(barControl[columnIdx], barValue);

    display.refreshControl(barControl[columnIdx]);
}

bool cInstrumentEditor::isMacroBipolar(int synthPartIdx, int macroIdx)
{
    auto& patch = IE->getSynth()->getPatch(synthPartIdx);
    const auto& metadata = patch.getMetadata();

    return metadata.macros[macroIdx].bipolar;
}

#endif

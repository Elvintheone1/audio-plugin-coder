#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

//==============================================================================
class SpliceAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   public juce::Timer
{
public:
    SpliceAudioProcessorEditor (SpliceAudioProcessor&);
    ~SpliceAudioProcessorEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;
    void timerCallback() override;

private:
    //==========================================================================
    // Header bar — pure JUCE component above the WebView.
    // Contains a Load button (always works) and keeps drag-and-drop as a bonus.
    struct HeaderBar : public juce::Component,
                       public juce::FileDragAndDropTarget
    {
        std::function<void (const juce::File&)> onFileDrop;
        std::function<void()>                   onLoadClick;
        juce::String labelText { "no reel loaded" };

        HeaderBar()
        {
            loadButton.setButtonText ("Load File");
            loadButton.setColour (juce::TextButton::buttonColourId,  juce::Colour (0xFF2A2A2A));
            loadButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xFF3A3A5A));
            loadButton.setColour (juce::TextButton::textColourOffId,  juce::Colour (0xFFAAAAAA));
            loadButton.onClick = [this] { if (onLoadClick) onLoadClick(); };
            addAndMakeVisible (loadButton);
        }

        void resized() override
        {
            const int btnW = 80, pad = 6;
            loadButton.setBounds (getWidth() - btnW - pad, pad, btnW, getHeight() - pad * 2);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (dragging ? juce::Colour (0xFF1E2030) : juce::Colour (0xFF111111));

            g.setColour (dragging ? juce::Colours::white : juce::Colour (0xFF777777));
            g.setFont (juce::Font (12.0f));

            // Label to the left of the button
            auto textArea = getLocalBounds().reduced (12, 0)
                                            .withRight (getWidth() - 94);
            g.drawText (labelText, textArea, juce::Justification::centredLeft);

            // Bottom separator
            g.setColour (juce::Colour (0xFF222222));
            g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
        }

        // FileDragAndDropTarget (bonus — works when macOS delivers the drag here)
        bool isInterestedInFileDrag (const juce::StringArray& files) override
        {
            for (const auto& f : files)
            {
                auto ext = juce::File (f).getFileExtension().toLowerCase();
                if (ext == ".wav" || ext == ".aif"  || ext == ".aiff" ||
                    ext == ".mp3" || ext == ".ogg"  || ext == ".flac" ||
                    ext == ".caf" || ext == ".m4a")
                    return true;
            }
            return false;
        }

        void filesDropped (const juce::StringArray& files, int, int) override
        {
            dragging = false; repaint();
            for (const auto& f : files)
            {
                juce::File file (f);
                if (file.existsAsFile()) { if (onFileDrop) onFileDrop (file); break; }
            }
        }

        void fileDragEnter (const juce::StringArray&, int, int) override { dragging = true;  repaint(); }
        void fileDragExit  (const juce::StringArray&)           override { dragging = false; repaint(); }

    private:
        bool dragging = false;
        juce::TextButton loadButton;
    };

    HeaderBar header;

    // File chooser — stored as member to stay alive during async operation
    std::unique_ptr<juce::FileChooser> fileChooser;
    void launchFileChooser();

    //==========================================================================
    // 1. PARAMETER RELAYS (declared first → destroyed last)
    juce::WebSliderRelay ampAttackRelay  { "amp_attack"  };
    juce::WebSliderRelay ampDecayRelay   { "amp_decay"   };
    juce::WebSliderRelay ampSustainRelay { "amp_sustain" };
    juce::WebSliderRelay ampReleaseRelay { "amp_release" };

    juce::WebSliderRelay filterCutoffRelay { "filter_cutoff" };
    juce::WebSliderRelay filterResRelay    { "filter_res"    };
    juce::WebSliderRelay filterTypeRelay   { "filter_type"   };

    juce::WebSliderRelay fenvDepthRelay  { "fenv_depth"  };
    juce::WebSliderRelay fenvAttackRelay { "fenv_attack" };
    juce::WebSliderRelay fenvDecayRelay  { "fenv_decay"  };

    juce::WebSliderRelay ampAttackShapeRelay  { "amp_attack_shape"  };
    juce::WebSliderRelay ampDecayShapeRelay   { "amp_decay_shape"   };
    juce::WebSliderRelay ampReleaseShapeRelay { "amp_release_shape" };
    juce::WebSliderRelay fenvAttackShapeRelay { "fenv_attack_shape" };
    juce::WebSliderRelay fenvDecayShapeRelay  { "fenv_decay_shape"  };

    juce::WebSliderRelay tapeAgeRelay    { "tape_age"    };
    juce::WebSliderRelay wowFlutterRelay { "wow_flutter" };

    juce::WebSliderRelay randOctRelay    { "rand_oct"    };
    juce::WebSliderRelay randFifthRelay  { "rand_fifth"  };
    juce::WebSliderRelay randCutoffRelay { "rand_cutoff" };
    juce::WebSliderRelay randEnvRelay    { "rand_env"    };
    juce::WebSliderRelay randVolRelay    { "rand_vol"    };
    juce::WebSliderRelay randJitterRelay   { "rand_jitter"  };
    juce::WebSliderRelay randStutterRelay  { "rand_stutter" };

    juce::WebSliderRelay outputVolRelay { "output_vol" };
    juce::WebSliderRelay volDbRelay     { "vol_db"     };
    juce::WebSliderRelay pitchRelay     { "pitch"      };
    juce::WebSliderRelay rootNoteRelay  { "root_note"  };
    juce::WebSliderRelay speedRelay     { "speed"      };
    juce::WebSliderRelay panRelay       { "pan"        };
    juce::WebSliderRelay panRndRelay    { "pan_rnd"    };
    juce::WebSliderRelay bpmRelay       { "bpm" };

    juce::WebSliderRelay eqLowRelay     { "eq_low"      };
    juce::WebSliderRelay eqLowMidRelay  { "eq_low_mid"  };
    juce::WebSliderRelay eqHighMidRelay { "eq_high_mid" };
    juce::WebSliderRelay eqHighRelay    { "eq_high"     };

    juce::WebSliderRelay gridRelay         { "grid"          };
    juce::WebSliderRelay playbackModeRelay { "playback_mode" };
    juce::WebSliderRelay reelDirRelay      { "reel_dir"      };
    juce::WebSliderRelay sliceDirRelay     { "slice_dir"     };

    juce::WebToggleButtonRelay arpHoldRelay    { "arp_hold"    };
    juce::WebSliderRelay       arpPatternRelay { "arp_pattern" };

    // 2. WEBBROWSERCOMPONENT (destroyed middle)
    std::unique_ptr<juce::WebBrowserComponent> webView;

    // 3. PARAMETER ATTACHMENTS (declared last → destroyed first)
    std::unique_ptr<juce::WebSliderParameterAttachment> ampAttackAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> ampDecayAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> ampSustainAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> ampReleaseAttachment;

    std::unique_ptr<juce::WebSliderParameterAttachment> filterCutoffAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> filterResAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> filterTypeAttachment;

    std::unique_ptr<juce::WebSliderParameterAttachment> fenvDepthAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> fenvAttackAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> fenvDecayAttachment;

    std::unique_ptr<juce::WebSliderParameterAttachment> ampAttackShapeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> ampDecayShapeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> ampReleaseShapeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> fenvAttackShapeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> fenvDecayShapeAttachment;

    std::unique_ptr<juce::WebSliderParameterAttachment> outputVolAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> volDbAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> pitchAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> rootNoteAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> speedAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> panAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> panRndAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> bpmAttachment;

    std::unique_ptr<juce::WebSliderParameterAttachment> eqLowAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> eqLowMidAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> eqHighMidAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> eqHighAttachment;

    std::unique_ptr<juce::WebSliderParameterAttachment> gridAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> playbackModeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> reelDirAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> sliceDirAttachment;

    std::unique_ptr<juce::WebToggleButtonParameterAttachment> arpHoldAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment>       arpPatternAttachment;

    std::unique_ptr<juce::WebSliderParameterAttachment> tapeAgeAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> wowFlutterAttachment;

    std::unique_ptr<juce::WebSliderParameterAttachment> randOctAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> randFifthAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> randCutoffAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> randEnvAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> randVolAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> randJitterAttachment;
    std::unique_ptr<juce::WebSliderParameterAttachment> randStutterAttachment;

    //==========================================================================
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);

    SpliceAudioProcessor& audioProcessor;

    bool startupDialogShown = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpliceAudioProcessorEditor)
};

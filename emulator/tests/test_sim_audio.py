from __future__ import annotations

from emulator.tests.conftest import assert_scenario_ok


def test_sim_pwm_audio_device_records_tone_requests(run_emulator_scenario):
    result = run_emulator_scenario(
        r'''
        import os
        from lexxos_emu import LexxOsSim, VirtualAudioMode, VirtualBoard

        board = VirtualBoard.dev_pico(audio_mode="pwm")
        with LexxOsSim(os.environ["LEXXOS_SIM_LIB"], board) as sim:
            sim.boot()
            sim.run_until_output_contains("lexx:/>", timeout=5.0)

            sim.write_line("devices")
            output = sim.run_until_idle()
            assert "emu_pwm_audio" in output, output
            assert "| audio   | ready" in output, output

            sim.write_line("audio")
            output = sim.run_until_idle()
            assert "audio: emu_pwm_audio" in output, output
            assert "tone: yes" in output, output

            sim.write_line("audio stereo --frequency 1200 --duration 50 --volume 96")
            output = sim.run_until_idle()
            assert "audio: tone channel=stereo frequency=1200 duration=50 volume=96" in output, output

            assert len(board.audio.tone_requests) == 1, board.audio.tone_requests
            tone = board.audio.tone_requests[0]
            assert tone.mode == VirtualAudioMode.PWM, tone
            assert tone.frequency_hz == 1200, tone
            assert tone.duration_ms == 50, tone
            assert tone.volume == 96, tone

            sim.write_line("beep --frequency 1300 --duration 40 --volume 80")
            output = sim.run_until_idle()
            assert "beep: tone frequency=1300 duration=40 volume=80" in output, output

            assert len(board.audio.tone_requests) == 2, board.audio.tone_requests
            beep = board.audio.tone_requests[1]
            assert beep.mode == VirtualAudioMode.PWM, beep
            assert beep.frequency_hz == 1300, beep
            assert beep.duration_ms == 40, beep
            assert beep.volume == 80, beep

            sim.write_line("exit")
            sim.run_until_idle()
        '''
    )
    assert_scenario_ok(result)


def test_sim_adc_audio_device_and_off_mode(run_emulator_scenario):
    result = run_emulator_scenario(
        r'''
        import os
        from lexxos_emu import LexxOsSim, VirtualBoard

        with LexxOsSim(os.environ["LEXXOS_SIM_LIB"], VirtualBoard.dev_pico(audio_mode="adc")) as sim:
            sim.boot()
            sim.run_until_output_contains("lexx:/>", timeout=5.0)
            sim.write_line("devices")
            output = sim.run_until_idle()
            assert "emu_adc_audio" in output, output
            sim.write_line("exit")
            sim.run_until_idle()
        '''
    )
    assert_scenario_ok(result)


def test_sim_audio_can_be_disabled(run_emulator_scenario):
    result = run_emulator_scenario(
        r'''
        import os
        from lexxos_emu import LexxOsSim, VirtualBoard

        with LexxOsSim(os.environ["LEXXOS_SIM_LIB"], VirtualBoard.dev_pico(audio_mode="off")) as sim:
            sim.boot()
            sim.run_until_output_contains("lexx:/>", timeout=5.0)
            sim.write_line("audio")
            output = sim.run_until_idle()
            assert "audio: no primary audio device" in output, output
            sim.write_line("exit")
            sim.run_until_idle()
        '''
    )
    assert_scenario_ok(result)


def test_mplay_pcm_output_reaches_python_sink_when_available(run_emulator_scenario):
    result = run_emulator_scenario(
        r'''
        import os
        from lexxos_emu import LexxOsSim, VirtualBoard

        board = VirtualBoard.dev_pico(audio_mode="pwm", audio_output="off")
        with LexxOsSim(os.environ["LEXXOS_SIM_LIB"], board) as sim:
            sim.boot()
            sim.run_until_output_contains("lexx:/>", timeout=5.0)
            sim.write_line("mplay --test")
            output = sim.run_until_output_contains("lexx:/>", timeout=5.0)
            if "unknown program: mplay" in output:
                sim.write_line("exit")
                sim.run_until_idle()
                raise SystemExit(0)

            assert "PCM playback starts now" in output, output
            assert "PCM test frames:" in output, output
            assert board.audio.drain_count >= 1, board.audio.drain_count
            assert len(board.audio.pcm_buffers) > 0, board.audio.pcm_buffers
            assert max(buffer.peak_left for buffer in board.audio.pcm_buffers) > 0, board.audio.pcm_buffers

            sim.write_line("exit")
            sim.run_until_idle()
        '''
    )
    assert_scenario_ok(result)

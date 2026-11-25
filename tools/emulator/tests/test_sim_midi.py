from __future__ import annotations

from tools.emulator.tests.conftest import assert_scenario_ok


def test_midi_dump_playback_and_pcm_failure_cleanup(run_emulator_scenario):
    result = run_emulator_scenario(
        r'''
        import os
        import struct
        import tempfile
        from pathlib import Path
        from lexxos_emu import LexxOsSim, VirtualBoard


        def midi(*tracks):
            header = b'MThd' + struct.pack('>IHHH', 6, int(len(tracks) > 1), len(tracks), 96)
            return header + b''.join(b'MTrk' + struct.pack('>I', len(track)) + track for track in tracks)


        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'single.mid').write_bytes(midi(b'\x00\x90\x3c\x64\x60\x80\x3c\x00\x00\xff\x2f\x00'))
            (root / 'multi.mid').write_bytes(midi(
                b'\x00\xff\x51\x03\x07\xa1\x20\x60\xff\x2f\x00',
                b'\x00\x90\x3c\x64\x60\x80\x3c\x00\x00\xff\x2f\x00',
            ))
            (root / 'invalid.mid').write_bytes(b'not a midi file')
            board = VirtualBoard.dev_pico(sd_directory=root, sd_inserted=True,
                                         sd_capacity='256M', sd_filesystem='fat32',
                                         audio_mode='pwm', audio_output='off')
            play_pcm = board.audio.play_pcm_buffer
            reject = False

            def play_or_reject_pcm(**request):
                return False if reject else play_pcm(**request)

            board.audio.play_pcm_buffer = play_or_reject_pcm
            with LexxOsSim(os.environ['LEXXOS_SIM_LIB'], board) as sim:
                sim.boot()
                sim.run_until_output_contains('lexx:/>', timeout=5.0)

                def command(text):
                    before = (board.audio.stop_count, board.audio.drain_count, len(board.audio.pcm_buffers))
                    sim.write_line(text)
                    output = sim.run_until_output_contains('lexx:/>', timeout=10.0)
                    assert 'unknown program: mplay' not in output, output
                    delta = [board.audio.stop_count - before[0], board.audio.drain_count - before[1],
                             len(board.audio.pcm_buffers) - before[2]]
                    return output, delta

                output, delta = command('mplay')
                assert 'missing MIDI file path' in output and delta == [0, 0, 0], output
                output, delta = command('mplay /mnt/sd/missing.mid')
                assert 'mplayer: /mnt/sd/missing.mid:' in output and delta == [0, 0, 0], output
                output, delta = command('mplay /mnt/sd/invalid.mid')
                assert 'Failed to load midi file:' in output and delta == [0, 0, 0], output
                output, delta = command('mplay --dump /mnt/sd/single.mid')
                assert 'Dumped events: 3' in output and 'time_us=500000' in output and delta == [0, 0, 0], output
                output, delta = command('mplay --dump /mnt/sd/multi.mid')
                assert 'Dumped events: 5' in output and 'tr=0 tempo' in output and delta == [0, 0, 0], output
                output, delta = command('mplay /mnt/sd/single.mid')
                assert 'MIDI render stats' in output and 'Notes on/off: 1/1' in output, output
                assert delta[0] == 1 and delta[1] >= 1 and delta[2] > 0, delta
                reject = True
                output, delta = command('mplay /mnt/sd/single.mid')
                assert 'MIDI render failed' in output and delta == [1, 0, 0], (output, delta)
                reject = False
                output, delta = command('mplay --test')
                assert 'PCM test frames:' in output and delta[1] >= 1 and delta[2] > 0, (output, delta)
                reject = True
                output, delta = command('mplay --test')
                assert 'PCM test output failed' in output, output
                sim.write_line('exit')
                sim.run_until_idle()
        ''',
        timeout=30.0,
    )
    assert_scenario_ok(result)

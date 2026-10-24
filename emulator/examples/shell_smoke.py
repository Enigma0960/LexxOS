from __future__ import annotations
import argparse
from emulator.lexxos_emu import LexxOsSim, VirtualBoard


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--lib", required=True)
    parser.add_argument("--sd-image")
    args = parser.parse_args()

    board = VirtualBoard.dev_pico(sd_image=args.sd_image, sd_inserted=bool(args.sd_image))

    with LexxOsSim(args.lib, board) as sim:
        sim.set_sd_inserted(bool(args.sd_image))
        sim.boot()
        print(sim.run_until_output_contains("lexx:/>", timeout=5.0), end="")
        sim.write_line("help")
        print(sim.run_until_idle(), end="")
        sim.write_line("exit")
        print(sim.run_until_idle(), end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

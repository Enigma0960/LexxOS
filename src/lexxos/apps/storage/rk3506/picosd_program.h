#pragma once

namespace lexxos::program {

class ProgramManager;

} // namespace lexxos::program

namespace lexxos::platform::rk3506 {

void registerPicoSdProgram(lexxos::program::ProgramManager& programs);

} // namespace lexxos::platform::rk3506

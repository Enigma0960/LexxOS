#pragma once

namespace lexxos::program {

class ProgramManager;

} // namespace lexxos::program

namespace lexxos::app {

using ProgramRegistrar = void (*)(program::ProgramManager&);

int run();
int run(ProgramRegistrar registerPrograms);

} // namespace lexxos::app

#pragma once

namespace lexxos::program {

class ProgramManager;

} // namespace lexxos::program

namespace lexxos::apps::storage_programs {

void registerSdmmcProgram(program::ProgramManager& programs);

} // namespace lexxos::apps::storage_programs

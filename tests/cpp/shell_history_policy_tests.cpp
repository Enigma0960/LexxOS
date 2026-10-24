#include <cstdio>
#include <initializer_list>
#include <string>
#include <string_view>

#include <lexxos/shell/shell_history_policy.h>

int main() {
	using lexxos::program::ProgramArgumentList;
	using lexxos::program::ProgramArgumentSpec;
	using lexxos::shell::detail::hasSensitiveArguments;
	using lexxos::shell::detail::historyLineAllowed;
	constexpr ProgramArgumentSpec arguments[] = {
	    {.name = "token", .help = "test secret", .sensitive = true},
	};
	auto lookup = [](std::string_view name, void const* context) {
		return name == "test-login" && hasSensitiveArguments(*static_cast<ProgramArgumentList const*>(context));
	};
	ProgramArgumentList specs{arguments, 1};
	for (auto line : {
	         "wifi-connect TestNet FakeSecret",
	         " wifi-config set TestNet FakeSecret",
	         "\"wifi-connect\" TestNet FakeSecret",
	         "wi'fi-'connect TestNet FakeSecret",
	         "wifi\\-connect TestNet FakeSecret",
	         "> /tmp/log wifi-connect TestNet FakeSecret",
	         "wifi-connect --invalid FakeSecret",
	         "wifi-config show",
	         "test-login --token=FakeSecret",
	         "test-login -t FakeSecret",
	         "test-login 'unterminated",
	     }) {
		if (historyLineAllowed(line, lookup, &specs)) {
			std::fprintf(stderr, "Sensitive line accepted: %s\n", line);
			return 1;
		}
	}

	for (auto line : {"help", "echo ordinary history", "echo 'quoted text'", "cat /tmp/a > /tmp/b", "wifi-status"}) {
		if (!historyLineAllowed(line, lookup, &specs)) {
			return 2;
		}
	}

	if (historyLineAllowed("wifi-connect TestNet FakeSecret", nullptr, nullptr)) {
		return 3; // Also filter imported history on builds without Wi-Fi.
	}

	if (hasSensitiveArguments({})) {
		return 4;
	}

	// Large ordinary arguments must not require a large temporary stack buffer.
	std::string const longPlainToken(512, 'a');
	if (!historyLineAllowed("echo " + longPlainToken, lookup, &specs)) {
		return 5;
	}
	
	if (historyLineAllowed("echo " + longPlainToken + " wifi-connect secret", lookup, &specs)) {
		return 6;
	}

	if (historyLineAllowed("echo " + longPlainToken + " wi\"fi\"-connect secret", lookup, &specs)) {
		return 7;
	}

	// A decoded token that cannot fit is rejected (fail closed).
	if (historyLineAllowed("echo \"" + longPlainToken + "\"", lookup, &specs)) {
		return 8;
	}

	return 0;
}

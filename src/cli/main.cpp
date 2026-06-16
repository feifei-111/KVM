// kvm command-line tool.
//
// Skeleton entry point: it wires up argument parsing (CLI11) and an app
// description, but defines no subcommands yet. Subcommands (e.g. parsing /
// printing IR) are added later as the surrounding tooling lands.

#include <CLI/CLI.hpp>

int main(int argc, char** argv) {
  CLI::App app{"kvm -- KVM IR command-line tool"};
  app.set_version_flag("--version", "kvm 0.0.0");

  // No subcommands yet. Require one once they exist; for now running with no
  // arguments just prints help.
  if (argc == 1) {
    std::printf("%s", app.help().c_str());
    return 0;
  }

  CLI11_PARSE(app, argc, argv);
  return 0;
}

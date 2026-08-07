status: done
files_changed: [tests/CMakeLists.txt]
evidence:
  - what_was_done: |
      Verified all 5 unit tests already registered (P0-04). Added 2 integration test registrations
      (TestPipelineIntegration, TestMockMcuCurve) with EXISTS guards. enable_testing() already present.
      7+2 tests discoverable via ctest from build directory.
risks:
  - Build env has no g++ compiler — pre-existing issue not related to CMakeLists
  - Integration test sources don't exist yet (WI-015); EXISTS guards auto-activate when they land
decisions:
  - problem: Integration test sources don't exist yet | chosen: EXISTS guard pattern | reason: CMake configure succeeds now; auto-picks up when WI-015 creates source files

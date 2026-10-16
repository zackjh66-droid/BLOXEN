# Hollowwake0.4.5 verification in progress

This pre-release source snapshot has not yet completed verification. Hosted run38919276770 is building/testing it. No Xbox execution or Fortnite lobby success is claimed.

Observed Xbox0.4.4 failure: original dbghelp called GetSystemInfo at return RVA1221867. New bindings and TV-safe no-scroll layout are described in SYSTEMINFO-FIX.md and ACCOUNT-AND-NAVIGATION.md.

An earlier run38918559524 was cancelled to include the statically proven next GetVersion initializer. Run38918838454 failed compilation because legacy GetVersion is not exposed in UWP headers. The current implementation packs GetVersionExW results and retains a desktop legacy-API comparison test. These failed/cancelled runs do not establish success.

The final delivery report will contain actual results after the hosted job finishes. Website/backend/account data and previous0.4.4 package remain unchanged.

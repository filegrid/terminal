1. Back up the current repository state into `tmp` before changing anything.
2. Restore portable/runtime files to the current branch HEAD baseline and rebuild the portable artifact.
3. Run portable acceptance checks from a fresh path to determine whether the baseline itself passes.
4. If needed, reapply changes in small groups to isolate the regression source.

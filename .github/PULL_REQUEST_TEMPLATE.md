## Summary of Changes

- Briefly summarize the main changes in list format.

## Reason for Changes

- Explain the background and why this change is necessary.

## How to Test

- Describe how to verify the changes, e.g.:

```bash
colcon build --packages-up-to rmw_int2dds_cpp
colcon test --packages-select rmw_int2dds_cpp
colcon test-result --verbose
```

- Note any RMW conformance / interoperability suites run
  (`test_rmw_implementation`, `test_communication`, cross-vendor), with results.

## Checklist

- [ ] `colcon build` and `colcon test` pass, including linters (`-R lint`)
- [ ] Documentation updated if behavior or APIs changed
- [ ] `CHANGELOG.rst` updated if behavior or APIs changed

## Notes

- Any special considerations or important points for reviewers.

### CLA Agreement

- [ ] I have read and agree to the
      [Individual Contributor License Agreement (CLA)](../CLA-Individual.md).

By submitting this pull request, I agree to the terms of the CLA.
No additional signature is required unless explicitly requested.

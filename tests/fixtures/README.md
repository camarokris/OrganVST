# Synthetic fixtures

`tests/render_test.py` generates original sine-wave fixtures at test time. No
third-party samples are needed or distributed. A WAV with loop metadata is used
for both a sustained rank and a percussive rank, verifying that the definition's
percussive flag overrides loop metadata. Generated files stay in the build tree.

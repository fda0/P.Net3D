find src/ -type f -name '*.*' -exec sed --in-place 's/[[:space:]]\+$//' {} \+

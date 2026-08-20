.PHONY: build install rebuild open help import-db package release

help:
	@echo "ShowJCR macOS commands:"
	@echo "  make build       Build ShowJCR.app into build/"
	@echo "  make install     Build, sign, and copy to /Applications"
	@echo "  make package     Build and create dist/ShowJCR-<version>-macos.zip"
	@echo "  make release     Package and publish GitHub Release (fork remote)"
	@echo "  make import-db   Import FQBJCR2021-2023 CSV into jcr.db"
	@echo "  make rebuild     Alias of make build"
	@echo "  make open        Open installed app in /Applications"

build rebuild:
	@./scripts/macos-install.sh --build-only

import-db:
	@python3 ./scripts/import-fqbjcr.py

install:
	@./scripts/macos-install.sh

package:
	@./scripts/macos-package.sh

release:
	@./scripts/macos-release.sh

open:
	@open /Applications/ShowJCR.app

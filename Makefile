# Makefile — Linux OS Fundamentals Labs
# Author: Arun Singh | arunsingh.in@gmail.com
# OS Fundamental Labs based on Prof. Sorav Bansal, NPTEL, IIT Delhi lecture series
#
# Usage:
#   make            # build all labs
#   make lab N=01   # run a single lab
#   make docker     # start Docker sandbox
#   make vm         # start Vagrant VM
#   make clean      # remove binaries

.PHONY: all build lab testdata docker docker-shell vm vm-ssh clean help

LAB_SCRIPT  := ./scripts/run_lab.sh
BUILD_SCRIPT := ./scripts/build_all.sh

# ---- Default target ----
all: build

build:
	@bash $(BUILD_SCRIPT)

# Run a lab: make lab N=07
lab: build
	@if [ -z "$(N)" ]; then echo "Usage: make lab N=<01-40>"; exit 1; fi
	@bash $(LAB_SCRIPT) $(N)

# Generate 128 MB random test file for page-cache labs
testdata:
	@if [ ! -f testdata.bin ]; then \
	    echo "Generating 128 MB testdata.bin..."; \
	    dd if=/dev/urandom of=testdata.bin bs=1M count=128 status=progress; \
	else \
	    echo "testdata.bin already exists."; \
	fi

# ---- Docker targets ----
docker: testdata
	docker compose up -d
	@echo ""
	@echo "Container is up.  Run:  make docker-shell"

docker-shell:
	docker compose exec lab bash

docker-build:
	docker build -t os-labs .

docker-stop:
	docker compose down

# ---- Vagrant targets ----
vm-rsync:
	vagrant rsync

vm-stop:
	vagrant halt

clean:
	rm -rf bin/*
	@echo "Binaries removed. Run 'make build' to rebuild."

# ---- Help ----
help:
	@echo ""
	@echo "Linux OS Fundamentals Labs — make targets"
	@echo "  make               build all 40 labs"
	@echo "  make lab N=01      run lab 01"
	@echo "  make lab N=27      run demand paging lab"
	@echo "  make testdata      generate 128 MB testdata.bin"
	@echo ""
	@echo "  Docker sandbox (no VM, instant start):"
	@echo "  make docker        build image + start container"
	@echo "  make docker-shell  open bash inside container"
	@echo "  make docker-stop   stop container"
	@echo ""
	@echo "  Vagrant VM (full Linux kernel, all labs):"
	@echo "  make vm            vagrant up (first run: provisions)"
	@echo "  make vm-ssh        open shell in VM"
	@echo "  make vm-rsync      push local changes to VM"
	@echo "  make vm-stop       halt VM"
	@echo ""
	@echo "  make clean         remove built binaries"
	@echo ""

# ── VM helpers (bug-fixed) ─────────────────────────────────────────
VM_NAME       := os-labs
VAGRANT_ID    := .vagrant/machines/default/virtualbox/id
INSECURE_KEY  := $(HOME)/.vagrant.d/insecure_private_key

.PHONY: vm
vm:
	@UUID=$$(VBoxManage list vms 2>/dev/null | grep '"$(VM_NAME)"' | grep -oE '\{[^}]+\}' | tr -d '{}'); \
	if [ -z "$$UUID" ]; then \
		echo "==> No existing VM found. Full provision starting..."; \
		vagrant up; \
	else \
		echo "==> VM '$(VM_NAME)' found (UUID: $$UUID)"; \
		mkdir -p $$(dirname $(VAGRANT_ID)); \
		echo "$$UUID" > $(VAGRANT_ID); \
		STATE=$$(VBoxManage showvminfo "$(VM_NAME)" 2>/dev/null | grep "^State:" | awk '{print $$2}'); \
		if [ "$$STATE" = "running" ]; then \
			echo "==> VM already running. Ready."; \
		else \
			echo "==> Resuming VM (state=$$STATE, no reprovision)..."; \
			vagrant up --no-provision; \
		fi \
	fi

.PHONY: vm-ssh
vm-ssh:
	@vagrant ssh 2>/dev/null || \
	ssh -i $(INSECURE_KEY) \
	    -o StrictHostKeyChecking=no \
	    -o UserKnownHostsFile=/dev/null \
	    -p 2222 vagrant@127.0.0.1

.PHONY: vm-status
vm-status:
	@echo "=== VirtualBox ===" && \
	VBoxManage showvminfo "$(VM_NAME)" 2>/dev/null | grep -E "State|Memory|CPUs" || echo "VM not found"
	@echo "=== SSH ===" && \
	ssh -i $(INSECURE_KEY) -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
	    -o ConnectTimeout=5 -p 2222 vagrant@127.0.0.1 "echo SSH_OK" 2>/dev/null || echo "SSH FAILED"

.PHONY: vm-destroy
vm-destroy:
	@read -p "Destroy VM? This cannot be undone. Type yes: " C; \
	if [ "$$C" = "yes" ]; then \
		vagrant destroy -f 2>/dev/null || true; \
		VBoxManage unregistervm "$(VM_NAME)" --delete 2>/dev/null || true; \
		rm -rf .vagrant/; \
		echo "==> VM destroyed. Run: make vm to reprovision."; \
	fi
# ──────────────────────────────────────────────────────────────────

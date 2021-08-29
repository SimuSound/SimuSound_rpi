#!/bin/bash -x

set -e

RULES_FILE="edgetpu-accelerator.rules"

function info {
  echo -e "\033[0;32m${1}\033[0m"  # green
}

function warn {
  echo -e "\033[0;33m${1}\033[0m"  # yellow
}

function error {
  echo -e "\033[0;31m${1}\033[0m"  # red
}

function install_file {
  local name="${1}"
  local src="${2}"
  local dst="${3}"

  info "Installing ${name} [${dst}]..."
  if [[ -f "${dst}" ]]; then
    warn "File already exists. Replacing it..."
    rm -f "${dst}"
  fi
  cp -a "${src}" "${dst}"
}

if [[ "${EUID}" != 0 ]]; then
  error "Please use sudo to run as root."
  exit 1
fi

install_file "device rule file" \
                "${RULES_FILE}" \
                "/etc/udev/rules.d/99-edgetpu-accelerator.rules"
udevadm control --reload-rules && udevadm trigger
info "Done."
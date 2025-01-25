#!/bin/sh

# PhreakScript
# (C) 2021-2025 Naveen Albert, PhreakNet, and others - https://github.com/InterLinked1/phreakscript ; https://portal.phreaknet.org ; https://docs.phreaknet.org
# v1.2.2 (2025-01-24)

# Setup (as root):
# cd /usr/local/src
# wget https://raw.githubusercontent.com/InterLinked1/phreakscript/master/phreaknet.sh
# chmod +x phreaknet.sh
# ./phreaknet.sh make
# phreaknet update
# phreaknet install

## Begin Change Log:
# 2025-01-24 1.2.2 Asterisk: Target 22.2.0
# 2024-12-30 1.2.1 DAHDI Linux: Work around compilation failure for newer kernels
# 2024-11-03 1.2.0 Asterisk: Install Asterisk 22 by default
# 2024-10-16 1.1.8 wanpipe: Installation procedure improvements
# 2024-09-19 1.1.7 DAHDI: Slipstream critical build fixes, fix build issues on various distros and kernels
# 2024-09-16 1.1.6 DAHDI: Add patch to enable building of XPP drivers on 32-bit architectures
# 2024-09-15 1.1.5 DAHDI: Massive overhaul to DAHDI stop/start/restart logic, fixes for manual span assignment
# 2024-09-11 1.1.4 DAHDI: Target DAHDI 3.4.0, update patches
# 2024-03-17 1.1.3 DAHDI: Only build wanpipe if requested
# 2024-03-09 1.1.2 Asterisk: fix broken patches that no longer applied
# 2024-01-12 1.1.1 Asterisk: target Asterisk 21.1.0-rc1, fix 'phreaknet restart' command
# 2023-10-26 1.1.0 Asterisk/DAHDI: target Asterisk 21, DAHDI 3.3.0-rc1
# 2023-09-27 1.0.5 DAHDI: restore tor2 and pciradio drivers, PhreakScript: pull update script from GitHub
# 2023-09-16 1.0.4 DAHDI: Fix DAHDI driver restoral, fix wcte12xp compilation on kernels >= 5.16
# 2023-09-08 1.0.3 wanpipe: Use wanpipe 7.0.36
# 2023-08-30 1.0.2 wanpipe: Use wanpipe 7.0.35
# 2023-08-28 1.0.1 PhreakScript: remove gerrit commands
# 2023-07-27 1.0.0 Asterisk: target 20.4.0
# 2023-06-18 0.3.4 PhreakScript: add source command
# 2023-05-25 0.3.3 Asterisk: target 20.3.0
# 2023-03-16 0.3.2 Asterisk: target 20.2.0
# 2023-03-03 0.3.1 DAHDI: disable XPP drivers on all 32-bit architectures to fix build failures
# 2023-02-26 0.2.7 PhreakScript: fix install user (again)
# 2023-02-25 0.2.6 PhreakScript: fix install user
# 2023-02-13 0.2.5 Asterisk: remove merged patches
# 2023-01-28 0.2.4 PhreakScript: fix GDB installation detection
# 2023-01-17 0.2.3 DAHDI: Correct for DAHDI no longer including CONFIG_PCI and patch cdd6ddd0fd08cb8b7313b16074882439fbb58045 failing
# 2023-01-17 0.2.2 DAHDI: Correct for DAHDI no longer including CONFIG_PCI and patch 45ac6a30f922f4eef54c0120c2a537794b20cf5c failing
# 2023-01-12 0.2.1 Asterisk: target 20.1.0
# 2023-01-07 0.2.0 Asterisk: readd chan_sip support for master
# 2022-11-27 0.1.99 Asterisk/DAHDI: add app_loopdisconnect
# 2022-11-25 0.1.98 Asterisk: add unmerged patches
# 2022-11-20 0.1.97 Asterisk: update usecallmanager target
# 2022-11-16 0.1.96 Asterisk: add out of tree modules
# 2022-11-05 0.1.95 PhreakScript: add minimal external codec handling
# 2022-10-27 0.1.94 PhreakScript: add keyperms command
# 2022-10-27 0.1.93 Asterisk: add unmerged patches, PhreakScript: add threads command
# 2022-10-20 0.1.92 Asterisk: target Asterisk 20.0.0
# 2022-10-15 0.1.91 PhreakScript: add reftrace command
# 2022-09-28 0.1.90 DAHDI: remove merged DAHDI compiler fix, add libpri compiler fix
# 2022-09-16 0.1.89 Asterisk: add unmerged patches
# 2022-09-03 0.1.88 Asterisk: add unmerged patches
# 2022-09-03 0.1.87 DAHDI: Add support for Raspberry Pi
# 2022-09-02 0.1.86 Test Suite: upgraded for python3
# 2022-08-31 0.1.85 DAHDI: support kernel version mismatches
# 2022-08-25 0.1.84 PhreakScript: improved rundump
# 2022-08-06 0.1.83 PhreakScript: added version command
# 2022-07-30 0.1.82 PhreakScript: streamline prereq install
# 2022-07-29 0.1.81 DAHDI: fix wanpipe compiling, target DAHDI 3.2.0
# 2022-07-24 0.1.80 Asterisk: remove merged patches
# 2022-07-20 0.1.79 Asterisk: fix memory issue in app_signal
# 2022-07-20 0.1.78 PhreakScript: add fullpatch command
# 2022-07-11 0.1.77 PhreakScript: added package audit
# 2022-07-11 0.1.76 PhreakScript: streamline enhanced dependencies
# 2022-07-05 0.1.75 Asterisk: update usecallmanager target
# 2022-07-01 0.1.74 Asterisk: add res_irc
# 2022-07-01 0.1.73 PhreakScript: fix RSA chown
# 2022-06-26 0.1.72 PhreakScript: added wizard command
# 2022-06-23 0.1.71 Asterisk: target 18.13.0
# 2022-06-01 0.1.70 PhreakScript: fix patch conflicts
# 2022-05-17 0.1.69 Asterisk: readd hearpulsing
# 2022-05-17 0.1.68 PhreakScript: enhanced install control
# 2022-05-16 0.1.67 Asterisk: add func_query and app_callback
# 2022-05-14 0.1.66 PhreakScript: add trace notify and custom expiry
# 2022-05-12 0.1.65 Asterisk: target 18.12.0
# 2022-05-05 0.1.64 PhreakScript: enhance installation compatibility
# 2022-05-01 0.1.63 Asterisk: add app_predial
# 2022-04-26 0.1.62 PhreakScript: add restart command
# 2022-04-25 0.1.61 PhreakScript: remove antipatterns
# 2022-04-24 0.1.60 DAHDI: add critical DAHDI Tools fix
# 2022-04-07 0.1.59 PhreakScript: improved odbc installation
# 2022-04-07 0.1.58 PhreakScript: add autocompletion integration
# 2022-04-05 0.1.57 PhreakScript: added res_dialpulse, misc. fixes
# 2022-04-03 0.1.56 PhreakScript: move boilerplate configs to GitHub
# 2022-04-03 0.1.55 Asterisk: add app_selective
# 2022-04-01 0.1.54 PhreakScript: warn about updates only if behind master
# 2022-04-01 0.1.53 PhreakScript: allow standalone DAHDI install
# 2022-03-27 0.1.52 PhreakScript: added dialplanfiles
# 2022-03-25 0.1.51 PhreakScript: add fail2ban
# 2022-03-25 0.1.50 PhreakScript: add paste_post error handling
# 2022-03-20 0.1.49 Asterisk: add func_dtmf_flash
# 2022-03-17 0.1.48 PhreakScript: added swap commands
# 2022-03-11 0.1.47 DAHDI: fix compiler error in DAHDI Tools
# 2022-03-06 0.1.46 Asterisk: added app_featureprocess
# 2022-03-05 0.1.45 PhreakScript: added cppcheck
# 2022-03-04 0.1.44 PhreakScript: add apiban
# 2022-03-01 0.1.43 Asterisk: update Call Manager to 18.10
# 2022-02-25 0.1.42 Asterisk: Fix xmldocs bug with SET MUSIC AGI
# 2022-02-23 0.1.41 PhreakScript: add out-of-tree tests for app_assert
# 2022-02-23 0.1.40 PhreakScript: minor test suite fixes
# 2022-02-23 0.1.39 PhreakScript: minor refactoring
# 2022-02-11 0.1.38 Asterisk: refined freepbx support
# 2022-02-10 0.1.37 Asterisk: target 18.10.0
# 2022-02-05 0.1.36 Asterisk: add hearpulsing to DAHDI
# 2022-02-03 0.1.35 PhreakScript: added preliminary freepbx flag
# 2022-01-27 0.1.34 PhreakScript: added master branch install option
# 2022-01-22 0.1.33 Asterisk: removed func_frameintercept
# 2022-01-19 0.1.32 Asterisk: add app_playdigits
# 2022-01-18 0.1.31 Asterisk: Temper SRTCP warnings
# 2022-01-10 0.1.30 Asterisk: add res_coindetect
# 2022-01-08 0.1.29 Asterisk: add app_randomplayback
# 2022-01-07 0.1.28 Asterisk: add app_pulsar
# 2022-01-04 0.1.27 Asterisk: add app_saytelnumber
# 2022-01-01 0.1.26 PhreakScript: removed hardcoded paths
# 2021-12-31 0.1.25 PhreakScript: added ulaw command, Asterisk: added func_frameintercept, app_fsk
# 2021-12-27 0.1.24 PhreakScript: add tests for func_dbchan
# 2021-12-26 0.1.23 PhreakScript: added Asterisk build info to trace, Asterisk: added app_loopplayback, func_numpeer
# 2021-12-24 0.1.22 PhreakScript: fix path issues, remove upgrade command
# 2021-12-20 0.1.21 Asterisk: update target usecallmanager
# 2021-12-17 0.1.20 PhreakScript: add tests for verify, added backtrace enable
# 2021-12-16 0.1.19 PhreakScript: added support for building chan_sip with Cisco Call Manager phone support
# 2021-12-15 0.1.18 PhreakScript: added runtests, Asterisk: update func_evalexten
# 2021-12-14 0.1.17 Asterisk: update func_evalexten, PhreakScript: added gerrit command
# 2021-12-13 0.1.16 Asterisk: patch updates, compiler fixes
# 2021-12-12 0.1.15 Asterisk: add ReceiveText application
# 2021-12-12 0.1.14 Asterisk: add app_verify, PhreakScript: fix double compiling with test framework
# 2021-12-11 0.1.13 PhreakScript: update backtrace
# 2021-12-09 0.1.12 Asterisk: updates to target 18.9.0
# 2021-12-05 0.1.11 PhreakScript: allow overriding installed version
# 2021-12-04 0.1.10 Asterisk Test Suite: added sipp installation, bug fixes to PhreakScript directory check
# 2021-11-30 0.1.9 Asterisk: add chan_sccp channel driver
# 2021-11-29 0.1.8 PhreakScript: fix trace bugs and add error checking
# 2021-11-26 0.1.7 PhreakScript: added docgen
# 2021-11-26 0.1.6 Asterisk: app_tdd (Bug Fix): added patch to fix compiler warnings, decrease buffer threshold
# 2021-11-25 0.1.5 PhreakScript: removed unnecessary file deletion in dev mode
# 2021-11-25 0.1.4 PhreakScript: changed upstream for app_softmodem
# 2021-11-24 0.1.3 PhreakScript: refactored out-of-tree module code and sources
# 2021-11-14 0.1.2 Asterisk: chan_sip (New Feature): Add fax control using FAX_DETECT_SECONDS and FAX_DETECT_OFF variables, PhreakScript: path improvements
# 2021-11-12 0.1.1 PhreakScript: (PHREAKSCRIPT-1) fixed infinite loop with --help argument
# 2021-11-09 0.1.0 PhreakScript: changed make to use hard link instead of alias, FreeBSD linking fixes
# 2021-11-09 0.0.54 PhreakScript: lots and lots of POSIX compatibility fixes, added info command, flag test option
# 2021-11-08 0.0.53 PhreakScript: fixed realpath for POSIX compliance
# 2021-11-08 0.0.52 PhreakScript: POSIX compatibility fixes for phreaknet validate
# 2021-11-08 0.0.51 PhreakScript: compatibility changes to make POSIX compliant
# 2021-11-07 0.0.50 PhreakScript: added app_dialtone
# 2021-11-06 0.0.49 PhreakScript: added debug to trace
# 2021-11-03 0.0.48 PhreakScript: added basic dialplan validation
# 2021-11-02 0.0.47 PhreakScript: switched upstream Asterisk to 18.8.0
# 2021-11-01 0.0.46 PhreakScript: added boilerplate asterisk.conf
# 2021-10-30 0.0.45 PhreakScript: add IAX2 dynamic RSA outdialing
# 2021-10-25 0.0.44 PhreakScript: bug fixes to compiler options for menuselect, temp. bug fix for app_read and addition of func_json
# 2021-10-24 0.0.43 PhreakScript: temporarily added app_assert and sig_analog compiler fix
# 2021-10-16 0.0.42 PhreakScript: Added preliminary pubdocs command for Wiki-format documentation generation
# 2021-10-15 0.0.41 PhreakScript: remove chan_iax2 RSA patch (available upstream in 18.8.0-rc1)
# 2021-10-14 0.0.40 PhreakScript: Added GitHub integration
# 2021-10-14 0.0.39 Asterisk: change upstream Asterisk from 18.7 to 18.8.0-rc1, remove custom patches for logger, app_queue, CHANNEL_EXISTS, func_vmcount, app_mf (available upstream in 18.8.0-rc1)
# 2021-10-14 0.0.38 PhreakScript: Added update protection (against corrupted upstream) and ability to set custom upstream source for PhreakScript
# 2021-10-12 0.0.37 Asterisk: Pat Fleet sounds, boilerplate audio files, pulsar AGI
# 2021-10-12 0.0.34 DAHDI: Changed upstream from master to next branch by manually incorporating these patches
# 2021-10-08 0.0.2  Asterisk (ASTERISK-29681): chan_sip (New Feature): Add SIPAddParameter application and SIP_PARAMETER function
# 2021-10-08 0.0.2  Asterisk (ASTERISK-29496): app_mf (New Feature): Add SendMF application and PlayMF AMI action
# 2021-10-08 0.0.2  Asterisk (ASTERISK-29661): func_vmcount (Improvement): Add support for multiple mailboxes
# 2021-10-08 0.0.2  Asterisk (ASTERISK-29656): func_channel (New Feature): Add CHANNEL_EXISTS function
# 2021-10-08 0.0.2  Asterisk (ASTERISK-29578): app_queue (Bug Fix): Fix hints for included contexts
# 2021-10-08 0.0.2  Asterisk (ASTERISK-29529): logger (Improvement): Add custom logging
# 2021-10-07 0.0.1  Asterisk: app_tdd (New Feature): Add TddRx/TddTx applications and AMI events
# 2021-10-07 0.0.1  Asterisk: app_softmodem (New Feature): Add Softmodem application (will eventually be removed and replaced with a new module) # TODO
# 2021-10-07 0.0.1  Asterisk: func_dbchan (New Feature): Add DB_CHAN, DB_PRUNE functions
# 2021-10-07 0.0.1  Asterisk: app_tonetest (New Feature): Add ToneSweep application
# 2021-10-07 0.0.1  Asterisk: app_streamsilence (New Feature): Add StreamSilence application
# 2021-10-07 0.0.1  Asterisk: app_frame (New Feature): Add SendFrame application
# 2021-10-07 0.0.1  Asterisk (ASTERISK-29486): func_evalexten (New Feature): Add EVAL_EXTEN function
# 2021-10-07 0.0.1  Asterisk: app_memory (New Feature): Add MallocTrim application
# 2021-10-07 0.0.1  Asterisk (ASTERISK-29489): app_mail (New Feature): Add SendMail application
# 2021-10-07 0.0.1  Asterisk: func_notchfilter (New Feature): Add NOTCH_FILTER function
# 2021-10-07 0.0.1  Asterisk (ASTERISK-29432): func_ochannel (New Feature): Add OTHER_CHANNEL function
# 2021-10-07 0.0.1  Asterisk (ASTERISK-29497): app_if (New Feature): Add If, EndIf, ExitIf applications
# 2021-10-07 0.0.1  Asterisk (ASTERISK-29428): app_dial (Bug Fix): prevent infinite loop from hanging calls when Dial D option used with progress
# 2021-10-07 0.0.1  Asterisk (ASTERISK-29455): translate.c (Bug Fix): prevent translator from choosing gsm to ulaw, all else equal
# 2021-10-07 0.0.1  Asterisk (ASTERISK-29493): app_stack (New Feature): Add ReturnIf application
# 2021-10-07 0.0.1  Asterisk (ASTERISK-20219): chan_iax2 (Improvement): Add encryption to RSA authentication (merged as of Oct. 2021, patch will be removed soon)
## End Change Log

# User environment variables
AST_CONFIG_DIR="/etc/asterisk"
AST_VARLIB_DIR="/var/lib/asterisk"
AST_SOURCE_PARENT_DIR="/usr/src"

# Script environment variables
AST_ALT_VER=""
AST_MIN_PREFERRED_VER=20
AST_DEFAULT_MAJOR_VER=22
AST_NEXT_MAJOR_VER=23
AST_MAJOR_VER=$AST_DEFAULT_MAJOR_VER
AST_MM_VER=0
AST_SOURCE_NAME="asterisk-${AST_DEFAULT_MAJOR_VER}-current"
AST_RC_SOURCE_NAME="asterisk-${AST_DEFAULT_MAJOR_VER}-testing"

# DAHDI_MM_VER="${DAHDI_VERSION:0:1}${DAHDI_VERSION:2:1}" Requires bash
DAHDI_MM_VER=34
DAHDI_VERSION="3.4.0"
#DAHLIN_SRC_URL="https://github.com/asterisk/dahdi-linux/releases/download/v${DAHDI_VERSION}/${DAHLIN_SRC_NAME}"
#DAHTOOL_SRC_URL="https://github.com/asterisk/dahdi-tools/releases/download/v${DAHDI_VERSION}/${DAHTOOL_SRC_NAME}"
DAHLIN_SRC_NAME="dahdi-linux-current.tar.gz"
DAHTOOL_SRC_NAME="dahdi-tools-current.tar.gz"
DAHLIN_SRC_URL="http://downloads.asterisk.org/pub/telephony/dahdi-linux/dahdi-linux-current.tar.gz"
DAHTOOL_SRC_URL="http://downloads.asterisk.org/pub/telephony/dahdi-tools/dahdi-tools-current.tar.gz"

LIBPRI_SOURCE_NAME="libpri-1.6.1"
LIBSS7_VERSION="2.0.1"
WANPIPE_SOURCE_NAME="wanpipe-current" # wanpipe-latest (7.0.38, 2024-02-05)
ODBC_VER="3.1.14"
CISCO_CM_SIP="cisco-usecallmanager-20.10.0"
MIN_ARGS=1
FILE_DIR="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
FILE_NAME=$( basename $0 ) # grr... why is realpath not in the POSIX standard?
FILE_PATH="$FILE_DIR/$FILE_NAME"
PATCH_DIR=https://docs.phreaknet.org/script
OS=$(uname -s)
OS_DIST_INFO="(lsb_release -ds || cat /etc/*release || uname -om ) 2>/dev/null | head -n1 | cut -d'=' -f2"
OS_DIST_INFO=$(eval "$OS_DIST_INFO" | tr -d '"')
PAC_MAN="apt-get"
AST_SOUNDS_DIR="$AST_VARLIB_DIR/sounds/en"
AST_MOH_DIR="$AST_VARLIB_DIR/moh"
AST_MAKE="make"
WGET="wget -q"
XMLSTARLET="/usr/bin/xmlstarlet"
PATH="/sbin:$PATH" # in case su used without path

# Defaults
AST_CC=1 # Country Code (default: 1 - NANPA)
AST_USER=""
EXTRA_FEATURES=1
ALSA=0
WEAK_TLS=0
CHAN_SIP=0
ENHANCED_CHAN_SIP=0
SIP_CISCO=0
CHAN_SCCP=0
CHAN_DAHDI=0
DAHDI_OLD_DRIVERS=0
EMPULSE=1 # Automatically enable EMPULSE, cause why not?
DAHDI_WANPIPE=0 # wanpipe only needed for Sangoma cards
DEVMODE=0
TEST_SUITE=0
FORCE_INSTALL=0
IGNORE_FAILURES=0
ENHANCED_INSTALL=1
PREFER_RELEASE_CANDIDATES=1
EXPERIMENTAL_FEATURES=0
LIGHTWEIGHT=0
FAST_COMPILE=0
PKG_AUDIT=0
MANUAL_MENUSELECT=0
ENABLE_BACKTRACES=0
ASTKEYGEN=0
DEFAULT_PATCH_DIR="/tmp" # for new patches
PHREAKNET_CLLI=""
INTERLINKED_APIKEY=""
BOILERPLATE_SOUNDS=0
SCRIPT_UPSTREAM="$PATCH_DIR/phreaknet.sh"
DEBUG_LEVEL=0
FREEPBX_GUI=0
GENERIC_HEADERS=0
AUTOSET_KVERS=0
EXTERNAL_CODECS=0
RTPULSING=1
HEARPULSING=1
HAVE_COMPATIBLE_SPANDSP=1
PACMAN_UPDATED=0 # Internal flag

handler() {
	kill $BGPID
}

# FreeBSD doesn't support this escaping, so just do a simple print.
echog() {
	if [ "$OS_DIST_INFO" = "FreeBSD" ]; then
		printf "%s\n" "$*" >&2;
	else
		printf "\e[32;1m%s\e[0m\n" "$*" >&2;
	fi
}
echoerr() { # https://stackoverflow.com/questions/2990414/echo-that-outputs-to-stderr
	if [ "$OS_DIST_INFO" = "FreeBSD" ]; then
		printf "%s\n" "$*" >&2;
	else
		printf "\e[31;1m%s\e[0m\n" "$*" >&2;
	fi
}

die() {
	echoerr "$1"
	exit 1
}

if [ "$OS" != 'Linux' -a "$OS" != 'NetBSD' -a "$OS" != 'OpenBSD' -a "$OS" != 'FreeBSD' -a "$OS" != 'DragonFly' ]; then
	die "Your OS ($OS) is definitely not supported... aborting."
fi

if [ "$OS_DIST_INFO" = "FreeBSD" ]; then
	PAC_MAN="pkg"
	AST_SOURCE_PARENT_DIR="/usr/local/src"
	AST_CONFIG_DIR="/usr/local/etc/asterisk"
	AST_MAKE="gmake"
	XMLSTARLET="/usr/local/bin/xml"
elif [ "$OS_DIST_INFO" = "Sangoma Linux" ]; then # the FreePBX distro...
	PAC_MAN="yum"
elif [ -f /etc/redhat-release ]; then
	PAC_MAN="yum"
elif [ "$OS_DIST_INFO" = "SLES" ]; then
	PAC_MAN="zypper"
elif [ "$OS_DIST_INFO" = "openSUSE Tumbleweed" ]; then
	PAC_MAN="zypper"
elif [ -r /etc/arch-release ]; then
	PAC_MAN="pacman"
elif [ ! -f /etc/debian_version ]; then # Default is Debian
	echoerr "Support for this platform ($OS_DIST_INFO) is limited... use at your own risk..."

	# Try to automatically detect the right package manager, at least...
	if which "yum" > /dev/null; then
		PAC_MAN="yum"
	elif which "dnf" > /dev/null; then
		PAC_MAN="dnf"
	elif which "pkg" > /dev/null; then
		PAC_MAN="pkg"
		AST_SOURCE_PARENT_DIR="/usr/local/src"
	elif ! which "apt-get" > /dev/null; then # apt-get is default, so check last
		echoerr "Failed to automatically determine your package manager... script will likely fail"
	fi
fi

update_packages() {
	printf "Updating package manager...\n"
	PACMAN_UPDATED=1
	if [ "$PAC_MAN" = "apt-get" ]; then
		if [ $PACMAN_UPDATED = 0 ]; then
			# Ubuntu 22.04 prompts for restarts by default, inhibit this: https://askubuntu.com/questions/1367139/apt-get-upgrade-auto-restart-services
			if [ -f /etc/needrestart/needrestart.conf ]; then
				sed -i 's/#$nrconf{restart} = '"'"'i'"'"';/$nrconf{restart} = '"'"'a'"'"';/g' /etc/needrestart/needrestart.conf
			fi
			if [ -f /var/cache/apt/pkgcache.bin ] && [ "$FORCE_INSTALL" != "1" ]; then
				echo $(( ($(date +%s) - $(stat /var/cache/apt/pkgcache.bin  -c %Y)) ))
				if [ $(( ($(date +%s) - $(stat /var/cache/apt/pkgcache.bin  -c %Y)) )) -lt 300 ]; then # within last 5 minutes
					printf "Package updates occured recently, skipping...\n"
					return
				fi
			fi
		fi
		apt-get update -y
		apt-get upgrade -y
	elif [ "$PAC_MAN" = "yum" ]; then
		yum update -y
	elif [ "$PAC_MAN" = "zypper" ]; then
		zypper update -y
	elif [ "$PAC_MAN" = "pacman" ]; then
		pacman -Syu --noconfirm
	elif [ "$PAC_MAN" = "pkg" ]; then
		pkg install -y
		pkg upgrade -y
	fi
}

install_package() {
	if [ $PACMAN_UPDATED -eq 0 ]; then
		update_packages
	fi
	echo "Installing package(s): " $1 # unquoted, to trim any leading whitespace
	if [ "$PAC_MAN" = "apt-get" ]; then
		apt-get install -y $1
	elif [ "$PAC_MAN" = "yum" ]; then
		yum install -y $1
	elif [ "$PAC_MAN" = "zypper" ]; then
		zypper install -y $1
	elif [ "$PAC_MAN" = "pacman" ]; then
		pacman -Sy --noconfirm $1
	elif [ "$PAC_MAN" = "pkg" ]; then
		pkg install -y $1
	else
		echoerr "Not sure how to satisfy requirement: $1"
	fi
	if [ $? -ne 0 ]; then
		echoerr "Package installation failed: $1"
	fi
}

# If which is not installed, get that before anything
if ! which "which" > /dev/null; then
	printf "which does not appear to be present... installing...\n"
	if [ "$PAC_MAN" = "apt-get" ]; then
		install_package "debianutils"
	elif [ "$PAC_MAN" = "yum" ]; then
		install_package "which"
	elif [ "$PAC_MAN" = "pacman" ]; then
		install_package "which"
	fi
	if ! which "which" > /dev/null; then
		echoerr "which is still not installed?"
	fi
fi

ensure_installed() {
	if ! which "$1" > /dev/null; then
		install_package "$1"
	fi
}

# Now that packages are detected, install wget if necessary
ensure_installed "wget"

# Wget2 does not support --show-progress, uses --force-progress instead
WGET_VERSION=$( wget --version | head -n 1 )
if [ "${WGET_VERSION#*"Wget2"}" != "$WGET_VERSION" ]; then
	WGET="$WGET --force-progress"
else
	WGET="$WGET --show-progress"
fi

if [ "$OS_DIST_INFO" = "Sangoma Linux" ]; then # the FreePBX distro...
	WGET="wget -q" # --show-progress not supported by yum/Sangoma Linux?
fi

# If getopt is not present, install it
if ! which "getopt" > /dev/null; then
	install_package "util-linux" # named the same in most every distro
fi
ensure_installed "hostname"

phreakscript_info() {
	printf "%s" "Hostname: "
	hostname
	printf "OS: %s\n" "$OS"
	printf "Dist Info: "
	echo $OS_DIST_INFO
	uname -a
	echo "Package Manager: $PAC_MAN"
	echo "wget version: $WGET_VERSION"
	gcc -v 2>&1 | grep "gcc version"
	asterisk -V 2> /dev/null # Asterisk might or might not exist...
	if [ -d /etc/dahdi ]; then
		dahdi_cfg -tv 2>&1 | grep "ersion"
	fi
	printf "%s" "PhreakScript "
	grep "# v" $FILE_PATH | head -1 | cut -d'v' -f2
	echo "https://github.com/InterLinked1/phreakscript"
	echo "(C) 2021-2024 PhreakNet - https://portal.phreaknet.org https://docs.phreaknet.org"
	echo "To report bugs or request feature additions, please report at https://issues.interlinked.us (also see https://docs.phreaknet.org/#contributions) and/or post to the PhreakNet mailing list: https://groups.io/g/phreaknet" | fold -s -w 120
}

if [ "$1" = "commandlist" ]; then
	echo "about help version examples info wizard make man mancached install source experimental dahdi odbc installts fail2ban apiban freepbx pulsar sounds boilerplate-sounds ulaw remsil uninstall uninstall-all bconfig config keygen keyperms update astpr patch genpatch alembic freedisk topdir topdisk enable-swap disable-swap start restart stop kill forcerestart ban applist funclist dialplanfiles validate trace paste iaxping pcap pcaps sngrep enable-backtraces backtrace backtrace-only rundump threads reftrace valgrind cppcheck docverify runtests runtest stresstest ccache fullpatch docgen mkdocs pubdocs edit"
	exit 0
fi

usage() {
	#complete -W "make help install installts pulsar sounds config keygen edit genpatch patch update upgrade trace backtrace" phreaknet # has to be manually entered, at present
	#source ~/.bash_profile
	echo "Usage: phreaknet [command] [options]
Commands:
   *** Getting Started ***
   about              About PhreakScript
   help               Program usage
   version            Program version
   examples           Example usages
   info               System info
   wizard             Interactive installation command wizard

   *** First Use / Installation ***
   make               Add PhreakScript to path
   man                Compile and install PhreakScript man page
   mancached          Install cached man page (may be outdated)
   install            Install or upgrade PhreakNet-enhanced Asterisk
   source             Prepare PhreakNet-enhanced Asterisk source code only
   experimental       Add experimental features to an existing Asterisk source
   dahdi              Install or upgrade PhreakNet-enhanced DAHDI
   wanpipe            Install wanpipe
   odbc               Install ODBC (MariaDB)
   installts          Install Asterisk Test Suite
   fail2ban           Install Asterisk fail2ban configuration
   apiban             Install apiban client
   freepbx            Install FreePBX GUI (not recommended)
   pulsar             Install Revertive Pulsing simulator
   sounds             Install Pat Fleet sound library
   boilerplate-sounds Install boilerplate sounds only
   ulaw               Convert wav to ulaw (specific file or all in current directory)
   remsil             Remove silence from file(s)
   uninstall          Uninstall Asterisk, but leave configuration behind
   uninstall-all      Uninstall Asterisk, and completely remove all traces of it (configs, etc.)

   *** Initial Configuration ***
   bconfig            Install PhreakNet boilerplate config
   config             Install PhreakNet boilerplate config and autoconfigure PhreakNet variables
   keygen             Install and update PhreakNet RSA keys
   keyperms           Ensure that TLS keys are readable

   *** Maintenance ***
   update             Update PhreakScript
   astpr              Apply an Asterisk PR patch
   patch              Patch PhreakNet Asterisk configuration
   genpatch           Generate a PhreakPatch
   alembic            Generate an Asterisk Alembic revision
   freedisk           Free up disk space
   topdir             Show largest directories in current directory
   topdisk            Show top files taking up disk space
   enable-swap        Temporarily allocate and enable swap file
   disable-swap       Disable and deallocate temporary swap file
   start              Fully start DAHDI, wanpipe, and Asterisk
   restart            Fully restart DAHDI, wanpipe, and Asterisk
   stop               Fully stop DAHDI, wanpipe, and Asterisk
   kill               Forcibly kill Asterisk
   forcerestart       Forcibly restart Asterisk
   ban                Manually ban an IP address using iptables

   *** Debugging ***
   dialplanfiles      Verify what files are being parsed into the dialplan
   validate           Run dialplan validation and diagnostics and look for problems
   trace              Capture a CLI trace and upload to InterLinked Paste
   paste              Upload an arbitrary existing file to InterLinked Paste
   iaxping            Check if a remote IAX2 listener is reachable
   pcap               Perform a packet capture, optionally against a specific IP address
   pcaps              Same as pcap, but open in sngrep afterwards
   sngrep             Perform SIP message debugging
   enable-backtraces  Enables backtraces to be extracted from the core dumper (new or existing installs)
   backtrace          Use astcoredumper to process a backtrace and upload to InterLinked Paste
   backtrace-only     Use astcoredumper to process a backtrace
   rundump            Get a backtrace from the running Asterisk process
   threads            Get information about current Asterisk threads
   reftrace           Process reference count logs

   *** Developer Debugging ***
   valgrind           Run Asterisk under valgrind
   cppcheck           Run cppcheck on Asterisk for static code analysis

   *** Development & Testing ***
   docverify          Show documentation validation errors and details
   runtests           Run differential PhreakNet tests
   runtest            Run any specified test (argument to command)
   stresstest         Run any specified test multiple times in a row
   fullpatch          Redownload an entire PhreakNet source file
   ccache             Globally install ccache to speed up recompilation

   *** Miscellaneous ***
   docgen             Generate Asterisk user documentation, using astdocgen (deprecated)
   mkdocs             Generate Asterisk user documentation, using Asterisk documentation generator
   pubdocs            Generate Asterisk user documentation (deprecated)
   applist            List Asterisk dialplan applications in current source
   funclist           List Asterisk dialplan functions in current source
   edit               Edit PhreakScript
   touch              Show PhreakScript file path and last modification

Options:
   -b, --backtraces   Enables getting backtraces
   -c, --cc           Country Code (default is 1)
   -d, --dahdi        Install DAHDI
   -f, --force        Force install or config
   -h, --help         Display usage
   -n, --no-rc        Do not install release candidate versions
   -o, --flag-test    Option flag test
   -s, --sip          Install chan_sip instead of or in addition to chan_pjsip
   -t, --testsuite    Compile with developer support for Asterisk test suite and unit tests
   -u, --user         User as which to run Asterisk (non-root)
   -v, --version      Specific version of Asterisk to install (M.m.b e.g. 18.8.0). Also, see --vanilla.
   -w, --weaktls      Allow less secure TLS versions down to TLS 1.0 (default is 1.2+)
       --api-key      config: InterLinked API key
       --clli         config: CLLI code
       --disa         config: DISA number
       --rotate       keygen: Rotate/create keys
       --upstream     update: Specify upstream source
       --debug        trace: Debug level (default is 0/OFF, max is 10)
       --boilerplate  sounds: Also install boilerplate sounds
       --audit        install: Audit package installation
       --devmode      install: Compile with devmode enabled
       --experimental install: Install experimental features that may not be production ready
       --fast         install: Compile as fast as possible
       --lightweight  install: Only install basic, required modules for basic Asterisk functionality
       --alsa         install: Ensure ALSA library detection exists in the build system. This does NOT readd the deprecated/removed chan_alsa module.
       --cisco        install: Add full support for Cisco Call Manager phones (chan_sip only)
       --sccp         install: Install chan_sccp channel driver (Cisco Skinny)
       --drivers      install: Also install DAHDI drivers removed in 2018
       --generic      install: Use generic kernel headers that do not match the installed kernel version
       --autokvers    install: Automatically pass the appropriate value for KVERS for DAHDI compilation (only needed on non-Debian systems)
       --extcodecs    install: Specify this if any external codecs are being or will be installed
       --freepbx      install: Install FreePBX GUI (not recommended)
       --manselect    install: Manually run menuselect yourself
       --minimal      install: Do not upgrade the kernel or install nonrequired dependencies (such as utilities that may be useful on typical Asterisk servers)
       --vanilla      install: Do not install extra features or enhancements. Bug fixes are always installed. (May be required for older versions)
"
	phreakscript_info
	exit 2
}

get_newest_astdir() {
	if [ "$OS_DIST_INFO" = "FreeBSD" ]; then
		ls -d ./*/ | cut -c 3- | grep "^asterisk" | tail -1 # FreeBSD doesn't have the same GNU ls options: https://stackoverflow.com/a/31603260/
	else
		ls -d -v */ | grep "^asterisk" | tail -1
	fi
}

cd_ast() {
	cd $AST_SOURCE_PARENT_DIR
	AST_SRC_DIR=`get_newest_astdir`
	cd $AST_SRC_DIR
}

ast_kill() {
	pid=`cat /var/run/asterisk/asterisk.pid`
	if [ ${#pid} -eq 0 ]; then
		echoerr "Asterisk is not currently running."
	elif ! kill -9 $pid; then
		echoerr "Failed to stop Asterisk ($pid)"
	else
		echog "Successfully killed Asterisk ($pid)"
	fi
}

start_wanpipe() {
	if which wanrouter > /dev/null; then
		printf "Starting wanpipe\n"
		service wanrouter start
		if [ $? -ne 0 ]; then
			wanrouter start
		fi
		wanrouter status
	fi
}

stop_wanpipe() {
	if which wanrouter > /dev/null; then
		service wanrouter status
		if [ $? -eq 0 ]; then
			printf "Stopping wanpipe spans\n"
			if ! wanrouter stop all; then # stop all T1 spans on wanpipe
				die "Failed to stop wanpipe spans"
			elif ! service wanrouter stop; then # stop wanpipe service
				die "Failed to stop wanrouter"
			elif ! modprobe -r dahdi_echocan_mg2; then # remove DAHDI echocan
				die "Failed to remove DAHDI echocan"
			fi
		else
			wanrouter status
			if [ $? -eq 0 ]; then
				printf "Unsure if wanpipe is in use, stopping it just to be sure...\n"
				wanrouter stop all
				if [ $? -ne 0 ]; then
					die "Failed to stop wanpipe, aborting to prevent potential system crash - please manually stop wanpipe and rerun"
				fi
			fi
		fi
		wanrouter status | grep "stopped"
		if [ $? -ne 0 ]; then
			die "Could not verify wanpipe is really stopped... to prevent system instability, please manually stop it and rerun"
		fi
	else
		printf "wanpipe not present on this system, no need to stop it\n"
	fi
}

stop_telephony() {
	astpid=$( ps -aux | grep "asterisk" | grep -v "grep" | head -n 1 | xargs | cut -d' ' -f2 )
	if [ "$astpid" != "" ]; then
		if [ "$1" = "1" ]; then
			# Only need to unload chan_dahdi if it's loaded
			rasterisk -x "module show like chan_dahdi" | grep "chan_dahdi"
			if [ $? -eq 0 ]; then
				rasterisk -x "module unload chan_dahdi" | grep "Unloaded chan_dahdi"
				if [ $? -ne 0 ]; then
					die "chan_dahdi could not be unloaded"
				fi
			fi
		else
			service asterisk stop # stop Asterisk
			astpid=$( ps -aux | grep "asterisk" | grep -v "grep" | head -n 1 | xargs | cut -d' ' -f2 )
			if [ "$astpid" != "" ]; then
				# if that didn't work, kill it manually
				kill -9 $astpid
				printf "Killed Asterisk process %s\n" "$astpid"
			else
				printf "Asterisk not currently running...\n"
			fi
		fi
	else
		printf "Asterisk not currently running...\n"
	fi
	lsmod | grep dahdi
	curdrivers=`lsmod | grep "dahdi " | xargs | cut -d' ' -f4-` # Space intentionally included, add dahdi_vpmadt032_loader later manually
	printf "Current drivers: --- %s ---\n", "$curdrivers"
	stop_wanpipe
	echo "$curdrivers" | tr ',' '\n' | xargs -i sh -c 'echo Stopping driver: {}; modprobe -r {}'
	modprobe -r dahdi_vpmadt032_loader
	modprobe -r dahdi_voicebus
	service dahdi status
	# WARNING WARNING WARNING
	# It seems that on a system where wanpipe is in use,
	# stopping DAHDI without ensuring wanpipe is really stopped
	# will lead to the system crashing
	# Noticed this since wanpipe wasn't registered as a service,
	# so "service wanpipe status" wouldn't work, even though wanpipe was installed.
	# This is why there are now checks above to ensure wanrouter is stopped before stopping DAHDI.
	# Also noted here: https://sangomakb.atlassian.net/wiki/spaces/TC/pages/53772451/Driver+Debugging
	if [ $? -eq 0 ]; then
		printf "Stopping DAHDI...\n"
		if ! service dahdi stop; then
			printf "Returned %d\n" $?
			die "Failed to stop DAHDI"
		elif ! modprobe -r dahdi; then
			die "Failed to remove DAHDI from kernel"
		elif ! service dahdi stop; then # do it again, just to be sure
			die "Failed to stop DAHDI the second time"
		fi
		printf "DAHDI shutdown complete\n"
	else
		printf "DAHDI is not running, skipping...\n"
		modprobe -r dahdi # In case the service was already stopped but dahdi module was not yet unloaded
	fi
}

# Completely restart wanpipe, DAHDI (and any DAHDI drivers), and Asterisk
# This is surprisingly complicated, and can be dangerous if done incorrectly
# $1 to restart without completely restarting Asterisk
restart_telephony() {
	stop_telephony "$1"
	start_telephony
}

# Mainly intended to start the telephony drivers on bootup, since this doesn't always happen automatically
start_telephony() {
	if [ ! -f /etc/udev/rules.d/dahdi.rules ]; then
		echoerr "DAHDI udev rules are missing..."
	fi

	modprobe --first-time dahdi
	if [ $? -ne 0 ]; then
		echoerr "DAHDI is still running... stop these modules and try again"
		lsmod | grep "dahdi" | xargs | cut -d' ' -f4- # No space in this grep
		exit 1
	fi
	start_wanpipe # This can fail, if wanpipe isn't being used, and that's fine

	printf "DAHDI hardware:\n"
	dahdi_hardware # List DAHDI hardware. The dahdi module must first be running for this to work.

	# Start drivers for any telephony cards, which is needed for span assignment
	# sed: Each one ends in - or +, need to ignore that
	for driver in $( dahdi_hardware | awk '{print $2}'  | tr ',' '\n' | sed 's/.$//' ) ; do
		printf "Starting driver: %s... " "$driver"
		modprobe --first-time $driver
		if [ $? -ne 0 ]; then
			echoerr "\nFailed to start driver $driver"
		else
			printf "started!\n"
		fi
	done

	# Dump detected configuration since span stuff is VERY fickle and prone to breakage
	printf "DAHDI module options: "
	cat /etc/modprobe.d/dahdi.conf # Dump DAHDI module options
	AUTO_ASSIGN_SPANS=$( cat /sys/module/dahdi/parameters/auto_assign_spans | tr -d '\n' )
	printf "DAHDI span auto-assignment: %d\n" "$AUTO_ASSIGN_SPANS"
	printf "Detected spans:\n"
	dahdi_span_assignments list # List detected spans

	# Get the spans assigned.
	printf "Assigning DAHDI spans...\n"
	dahdi_span_assignments remove
	if [ "$AUTO_ASSIGN_SPANS" = "0" ]; then
		printf "Manually assigning spans according to /etc/dahdi/assigned-spans.conf\n"
		dahdi_span_assignments add
	else
		printf "Automatically assigning spans, ignoring /etc/dahdi/assigned-spans.conf\n"
		num_spans=$( dahdi_span_assignments list | wc -l )
		if [ $num_spans -gt 1 ]; then
			echoerr "Detected that this machine has more than 1 DAHDI span.\nYou are HIGHLY ENCOURAGED to assign the span order explicitly in /etc/dahdi/assigned-spans.conf!"
			echoerr "To do this, add 'options dahdi auto_assign_spans=0' to /etc/modprobe.d/dahdi.conf and run 'phreaknet restart'"
			sleep 1
		fi
		dahdi_span_assignments auto
	fi

	# Run dahdi_genconf to validate that the spans are assigned properly.
	# However, don't actually replace /etc/dahdi/system.conf, since that
	# would overwrite span configuration and break the system setup...
	# just use a temporary configuration file to catch it and compare.
	printf "Generating DAHDI channel configuration and checking differences...\n"

	# XXX BUGBUG There seems to be a bug where if (and even if?) dahdi_span_assignments auto is not run
	# (even if we're manually assigning spans), dahdi_genconf will just hang.
	# We can kill it and all the gubbins DAHDI spawns with "pkill dahdi",
	# and then things seem to work.
	if [ -f /etc/dahdi/system.conf ]; then
		# Per the BUGBUG note above: If dahdi_genconf is still running after 2 seconds, kill it and proceed. Don't ask me to explain it, but it works...
		DAHDI_CONF_FILE=/tmp/system.conf dahdi_genconf system & (sleep 2; ps -aux | grep "dahdi_genconf" | grep -v "grep" > /dev/null && pkill dahdi && echoerr "Forcibly killed dahdi_genconf")
		# The second time around, it SHOULD work instantly and properly dump dahdi_genconf output into /tmp/system.conf
		DAHDI_CONF_FILE=/tmp/system.conf dahdi_genconf system & (sleep 2; ps -aux | grep "dahdi_genconf" | grep -v "grep" > /dev/null && pkill dahdi && echoerr "Forcibly killed dahdi_genconf a second time???")
		# This will show us what's different between the system.conf dahdi_genconf just wrote to /tmp/system.conf
		# and the actual /etc/dahdi/system.conf
		# There are likely to be a few changes, but there shouldn't be anything major if everything went well.
		# If there's a big deviation, then manual intervention is likely required.
		diff -U 0 /etc/dahdi/system.conf /tmp/system.conf # Output with no context, since basically every line is unique already
	else
		dahdi_genconf -vvvvv
	fi

	# Finally, run dahdi_cfg
	printf "Applying DAHDI channel configuration...\n"
	dahdi_cfg -v 2>/dev/null | grep "to configure" # Show number of channels that will be configured
	if [ $? -ne 0 ]; then
		lsdahdi
		die "DAHDI initialization failed... check to ensure all your spans are online and in the expected order"
	fi
	printf "DAHDI channel configuration applied successfully!\n"

	# Finally, make sure the DAHDI service is running so that systemd can keep track of it...
	service dahdi start

	service asterisk start # Start Asterisk if it's not running already
	/sbin/rasterisk -x "module load chan_dahdi" # Load chan_dahdi if Asterisk was already running
	/sbin/rasterisk -x "dahdi show channels" # The ultimate test is what DAHDI channels actually show up in Asterisk
	echog "Telephony initialization completed"
}

assert_root() {
	if [ $(id -u) -ne 0 ]; then
		echoerr "PhreakScript make must be run as root. Aborting..."
		exit 2
	fi
}

download_if_missing() {
	if [ ! -f "$1" ]; then
		wget "$2"
	else
		printf "Audio file already present, not overwriting: %s\n" "$1"
	fi
}

make_file_readable() { # $1 = file to make readable.
	bottomdir=`dirname "$1"`
	realfilename=`printf '%s' "$1" | xargs | cut -d' ' -f 1`
	if [ ! -f "$realfilename" ]; then
		echoerr "File $realfilename does not exist"
		# If the file doesn't exist, forget about it.
		return
	fi

	newfilename=`realpath $realfilename`
	if [ "${#newfilename}" -gt 0 ]; then
		# If we have realpath, follow symlinks too.
		if [ "$newfilename" != "$realfilename" ]; then
			printf "Followed symlink from %s to %s\n" "$realfilename" "$newfilename"
			realfilename="$newfilename"
		fi
	else
		echoerr "realpath is not supported on this system"
	fi

	printf "chmod -R +r %s\n" "$bottomdir"
	chmod -R +r "$bottomdir" # This directory should contain only keys.
	cd "$bottomdir"
	if [ $? -eq 0 ]; then
		chmod +r "$realfilename" # Make the file itself readable.
		# Make all its parent directories readable.
		# FYI: Stop when we get to /etc, because if you chmod 744 /etc, you will have a VERY BAD DAY.
		# If you're reading this and already having a bad day, do chmod 755 /etc and that will fix it.
		# You need +x permissions to go into directories, not just read permissions.
		while [ `pwd` != "/" ] && [ `pwd` != "/etc" ] ; do
			ls -dla $curdir
			curdir=`pwd`
			printf "chmod 755 %s\n" "$curdir"
			cd ..
			chmod 755 $curdir
			ls -dla $curdir
			if [ $? -ne 0 ]; then
				echoerr "Failed to set key permissions"
				break
			fi
		done
	fi
}

make_keys_readable() {
	# Bad things will happen if Asterisk cannot read the TLS keys that it needs.
	# This will do a directory traversal to the location(s) of TLS keys and make sure
	# that the keys and all their parent directories are readable. If any parent
	# directory in the chain doesn't have the right permissions then things will fail.

	# Save the current working directory.
	curdir=`pwd`

	# grep commands: Don't show filename in output. Ignore files with <. Remove leading whitespace, eliminate lines beginning with ; (comment), take only the first word

	# tlscertfile used by http.conf and sip.conf
	# Simpler methods of looping on each line of output only work in bash and not POSIX sh
	echo -n "" > /tmp/astkeylist.txt
	grep -h -R "tlscertfile" $AST_CONFIG_DIR | grep -v '<' | cut -d'=' -f 2 | sed 's/^[ \t]*//' | sed '/^;/d' | grep -e ".pem" -e ".key" | cut -d' ' -f 1 >> /tmp/astkeylist.txt
	grep -h -R "priv_key_file" $AST_CONFIG_DIR | grep -v '<' | cut -d'=' -f 2 | sed 's/^[ \t]*//' | sed '/^;/d' | grep -e ".pem" -e ".key" | cut -d' ' -f 1 >> /tmp/astkeylist.txt

	while read filename
	do
		printf "Processing potential key: %s\n" "$filename"
		make_file_readable "$filename"
	done < /tmp/astkeylist.txt # POSIX compliant
	rm /tmp/astkeylist.txt

	cd $curdir # Restore original working directory.
}

# $1 = include Asterisk-only pre-reqs (not needed for DAHDI-only builds)
install_prereq() {
	# wget should already be installed at this point, so it's not included here
	PREREQ_PACKAGES=""
	RHEL_MAJOR_VERSION_8=0
	printf "Installing prerequisites for %s..." "$OS_DIST_INFO"
	# Even if we are just installing DAHDI (without Asterisk), $CHAN_DAHDI should be set to 1 at this point.
	# libnewt-dev is needed for newt, which dahdi_tool requires. If it's not available, it won't get built.
	# dwarves is needed for pahole, which DAHDI Linux install needs for BTF generation
	if [ "$PAC_MAN" = "apt-get" ]; then
		PREREQ_PACKAGES="$PREREQ_PACKAGES git patch gcc pkg-config autoconf automake m4 libtool build-essential"
		if [ "$CHAN_DAHDI" = "1" ]; then
			PREREQ_PACKAGES="$PREREQ_PACKAGES libnewt-dev dwarves"
		fi
		if [ "$1" = "1" ]; then
			PREREQ_PACKAGES="$PREREQ_PACKAGES curl subversion libcurl4-openssl-dev"
			if [ "$ENHANCED_INSTALL" = "1" ]; then
				PREREQ_PACKAGES="$PREREQ_PACKAGES dnsutils bc mpg123 ntp tcpdump festival"
			fi
			if [ "$DEVMODE" = "1" ]; then
				PREREQ_PACKAGES="$PREREQ_PACKAGES xmlstarlet" # only needed in developer mode for doc validation.
			fi
		fi
		PREREQ_PACKAGES="$PREREQ_PACKAGES libedit-dev" # Ubuntu also needs this package
		# apt-get install libcurl3-gnutls=7.64.0-4+deb10u2 # fix git clone not working: upvoted comment at https://superuser.com/a/1642989
		PREREQ_PACKAGES="$PREREQ_PACKAGES debconf-utils" # used to feed the country code non-interactively
	elif [ "$PAC_MAN" = "yum" ]; then
		# Format is something like: Rocky Linux release 8.10 (Green Obsidian)
		RHEL_MAJOR_VERSION=$( cat /etc/redhat-release | cut -d'(' -f1 | awk '{print $(NF)}' | cut -d'.' -f1 )
		if [ -f /etc/redhat-release ] && [ "$RHEL_MAJOR_VERSION" = "8" ]; then # RHEL or Rocky Linux major version 8
			RHEL_MAJOR_VERSION_8=1
		fi
		PREREQ_PACKAGES="$PREREQ_PACKAGES git patch gcc gcc-c++ pkg-config autoconf automake m4 libtool"
		if [ "$CHAN_DAHDI" = "1" ]; then
			PREREQ_PACKAGES="$PREREQ_PACKAGES newt-devel"
			if [ $RHEL_MAJOR_VERSION_8 -eq 0 ]; then
				PREREQ_PACKAGES="$PREREQ_PACKAGES dwarves" # Not available on 8.9
			fi
		fi
		if [ "$1" = "1" ]; then
			PREREQ_PACKAGES="$PREREQ_PACKAGES subversion libuuid-devel libxml2-devel sqlite-devel"
			if [ $RHEL_MAJOR_VERSION_8 -eq 0 ]; then
				PREREQ_PACKAGES="$PREREQ_PACKAGES libedit-devel" # Required on Fedora, may fail initially on Rocky Linux 8.9
			fi
		fi
	elif [ "$PAC_MAN" = "zypper" ]; then
		PREREQ_PACKAGES="$PREREQ_PACKAGES git-core make patch gawk subversion bzip2 gcc-c++"
		if [ "$CHAN_DAHDI" = "1" ]; then
			PREREQ_PACKAGES="$PREREQ_PACKAGES newt-devel dwarves"
		fi
		if [ "$1" = "1" ]; then
			# TODO Some of these should be in Asterisk's install_prereq script
			PREREQ_PACKAGES="$PREREQ_PACKAGES libedit-devel libuuid-devel libxml2-devel sqlite3-devel"
		fi
	elif [ "$PAC_MAN" = "pacman" ]; then
		PREREQ_PACKAGES="$PREREQ_PACKAGES git make patch gcc pkg-config autoconf automake m4 libtool"
		if [ "$CHAN_DAHDI" = "1" ]; then
			PREREQ_PACKAGES="$PREREQ_PACKAGES libnewt pahole"
		fi
		if [ "$1" = "1" ]; then
			PREREQ_PACKAGES="$PREREQ_PACKAGES subversion libedit"
		fi
	elif [ "$PAC_MAN" = "pkg" ]; then
		PREREQ_PACKAGES="$PREREQ_PACKAGES git gmake"
		if [ "$CHAN_DAHDI" = "1" ]; then
			PREREQ_PACKAGES="$PREREQ_PACKAGES newt dwarves"
		fi
		if [ "$1" = "1" ]; then
			PREREQ_PACKAGES="$PREREQ_PACKAGES curl subversion e2fsprogs-libuuid sqlite3 xmlstarlet libsysinfo"
			if [ "$ENHANCED_INSTALL" = "1" ]; then
				PREREQ_PACKAGES="$PREREQ_PACKAGES ntp tcpdump mpg123 bind-tools" # bind-tools for dig
			fi
		fi
	else
		echoerr "Could not determine what package manager to use..."
		return
	fi

	install_package "$PREREQ_PACKAGES"

	# Any followup work
	if [ "$PAC_MAN" = "yum" ]; then
		# Stop on RHEL systems without an active subscription since packages are failing to install
		if ! which git > /dev/null; then
			if [ -f /etc/redhat-release ]; then
				echoerr "Subscription required to use RHEL package manager"
			fi
			die "Git does not appear to be installed"
		fi
		if [ "$1" = "1" ]; then
			if [ $RHEL_MAJOR_VERSION_8 -eq 1 ]; then
				# Rocky Linux seems to be missing libedit-devel, and this package is "missing"
				printf "Installing libedit-devel from the devel repo\n"
				dnf --enablerepo=devel install -y libedit-devel
			fi
		fi
	elif [ "$PAC_MAN" = "pkg" ]; then
		if [ "$1" = "1" ]; then
			pkg info e2fsprogs-libuuid
			if [ $? -ne 0 ]; then
				if [ ! -d /usr/ports/misc/e2fsprogs-libuuid/ ]; then # https://www.freshports.org/misc/e2fsprogs-libuuid/
					portsnap fetch extract
				fi
				cd /usr/ports/misc/e2fsprogs-libuuid/ && make install clean # for uuid-dev
				pkg install misc/e2fsprogs-libuuid
			fi
		fi
	fi

	# Dump gcc version
	gcc -v 2>&1 | grep "gcc version"
	# /usr/local/src doesn't exist by default on FreeBSD
	if [ ! -d $AST_SOURCE_PARENT_DIR ]; then
		mkdir $AST_SOURCE_PARENT_DIR
	fi
}

install_boilerplate() {
	if [ ! -d $AST_CONFIG_DIR/dialplan ]; then
		mkdir -p $AST_CONFIG_DIR/dialplan
		cd $AST_CONFIG_DIR/dialplan
	elif [ ! -d $AST_CONFIG_DIR/dialplan/phreaknet ]; then
		mkdir -p $AST_CONFIG_DIR/dialplan/phreaknet
		cd $AST_CONFIG_DIR/dialplan/phreaknet
	else
		if [ "$FORCE_INSTALL" != "1" ]; then
			echoerr "It looks like you already have configs. Proceed and overwrite? (y/n) "
			printf '\a'
			read -r -p "" ans
			if [ "$ans" != "y" ]; then
				echo "Cancelling config installation."
				exit 1
			fi
		fi
	fi
	printf "Backing up %s configs, just in case...\n" $AST_CONFIG_DIR
	mkdir -p /tmp/etc_asterisk
	cp $AST_CONFIG_DIR/*.conf /tmp/etc_asterisk # do we really trust users not to make a mistake? backup to tmp, at least...
	EXTENSIONS_CONF_FILE="extensions.conf"
	if [ -f $AST_CONFIG_DIR/freepbx_module_admin.conf ]; then
		printf "%s\n" "Detected FreePBX installation..."
		EXTENSIONS_CONF_FILE="extensions_custom.conf"
	fi
	printf "%s" "Installing boilerplate code to "
	pwd
	printf "\n"
	$WGET https://raw.githubusercontent.com/InterLinked1/phreaknet-boilerplate/master/dialplan/verification.conf -O verification.conf --no-cache
	$WGET https://raw.githubusercontent.com/InterLinked1/phreaknet-boilerplate/master/dialplan/phreaknet.conf -O phreaknet.conf --no-cache
	$WGET https://raw.githubusercontent.com/InterLinked1/phreaknet-boilerplate/master/dialplan/phreaknet-aux.conf -O phreaknet-aux.conf --no-cache
	$WGET https://raw.githubusercontent.com/InterLinked1/phreaknet-boilerplate/master/dialplan/phreaknet-coin.conf -O phreaknet-coin.conf --no-cache
	cd $AST_CONFIG_DIR
	pwd
	$WGET https://raw.githubusercontent.com/InterLinked1/phreaknet-boilerplate/master/asterisk.conf -O $AST_CONFIG_DIR/asterisk.conf --no-cache
	$WGET https://raw.githubusercontent.com/InterLinked1/phreaknet-boilerplate/master/modules.conf -O $AST_CONFIG_DIR/modules.conf --no-cache
	$WGET https://raw.githubusercontent.com/InterLinked1/phreaknet-boilerplate/master/chan_dahdi.conf -O $AST_CONFIG_DIR/chan_dahdi.conf --no-cache
	$WGET https://raw.githubusercontent.com/InterLinked1/phreaknet-boilerplate/master/iax.conf -O $AST_CONFIG_DIR/iax.conf --no-cache
	$WGET https://raw.githubusercontent.com/InterLinked1/phreaknet-boilerplate/master/sip.conf -O $AST_CONFIG_DIR/sip.conf --no-cache
	$WGET https://raw.githubusercontent.com/InterLinked1/phreaknet-boilerplate/master/pjsip.conf -O $AST_CONFIG_DIR/pjsip.conf --no-cache
	$WGET https://raw.githubusercontent.com/InterLinked1/phreaknet-boilerplate/master/musiconhold.conf -O $AST_CONFIG_DIR/musiconhold.conf --no-cache
	$WGET https://raw.githubusercontent.com/InterLinked1/phreaknet-boilerplate/master/extensions.conf -O $AST_CONFIG_DIR/$EXTENSIONS_CONF_FILE --no-cache
	$WGET https://raw.githubusercontent.com/InterLinked1/phreaknet-boilerplate/master/verify.conf -O $AST_CONFIG_DIR/verify.conf --no-cache
	printf "%s\n" "Boilerplate config installed! Note that these files may still require manual editing before use."
}

install_boilerplate_sounds() {
	cd $AST_SOUNDS_DIR
	mkdir -p custom/signal
	cd custom/signal
	download_if_missing "dialtone.ulaw" "https://audio.phreaknet.org/stock/dialtone.ulaw"
	download_if_missing "busy.ulaw" "https://audio.phreaknet.org/stock/busy.ulaw"
	download_if_missing "reorder.ulaw" "https://audio.phreaknet.org/stock/reorder.ulaw"
	download_if_missing "ccad.ulaw" "https://audio.phreaknet.org/stock/ccad.ulaw"
	download_if_missing "acbn.ulaw" "https://audio.phreaknet.org/stock/acbn.ulaw"
	cd $AST_MOH_DIR
	mkdir -p ringback
	cd ringback
	download_if_missing "ringback.ulaw" "https://audio.phreaknet.org/stock/ringback.ulaw"
}

install_freepbx_checks() {
	if [ "$AST_USER" != "asterisk" ]; then
		echoerr "FreePBX requires installing as asterisk user (use --user asterisk)"
		exit 2
	fi
	if [ -f $AST_CONFIG_DIR/extensions.conf ]; then
		if [ "$FORCE_INSTALL" != "1" ]; then
			echoerr "An existing $AST_CONFIG_DIR/extensions.conf has been detected on the file system. Installing FreePBX will overwrite ALL OF YOUR CONFIGURATION!!!"
			printf "%s\n" "If this is intended, rerun the command with the -f or --force flag. Be sure to backup your configuration first if desired."
			exit 2
		fi
	fi
}

install_freepbx() { # https://www.atlantic.net/vps-hosting/how-to-install-asterisk-and-freepbx-on-ubuntu-20-04/
	FREEPBX_VERSION="freepbx-16.0-latest"
	# avoid using if possible
	# PHP 7.4 is supported: https://www.freepbx.org/freepbx-16-is-now-released-for-general-availability/
	apt-get -y install apache2 mariadb-server php libapache2-mod-php7.4 php7.4 php-pear php7.4-cgi php7.4-common php7.4-curl php7.4-mbstring php7.4-gd php7.4-mysql php7.4-bcmath php7.4-zip php7.4-xml php7.4-imap php7.4-json php7.4-snmp
	cd $AST_SOURCE_PARENT_DIR
	wget http://mirror.freepbx.org/modules/packages/freepbx/$FREEPBX_VERSION.tgz -O $AST_SOURCE_PARENT_DIR/$FREEPBX_VERSION.tgz
	tar -xvzf $FREEPBX_VERSION.tgz
	cd $AST_SOURCE_PARENT_DIR/freepbx
	rm ../$FREEPBX_VERSION.tgz
	if [ $? -ne 0 ]; then
		echoerr "Failed to find FreePBX source directory"
		return
	fi
	install_package "nodejs"
	phreak_tree_patch "installlib/installcommand.class.php" "freepbx_installvercheck.diff" # bug fix to installer
	./install -n
	if [ $? -ne 0 ]; then
		echoerr "Installation failed"
		return
	fi
	sed -i 's/^\(User\|Group\).*/\1 asterisk/' /etc/apache2/apache2.conf
	sed -i 's/AllowOverride None/AllowOverride All/' /etc/apache2/apache2.conf
	PHP_VERSION=`ls /etc/php | tr -d '\n'`
	printf "System version of PHP is %s\n" "$PHP_VERSION"
	sed -i 's/\(^upload_max_filesize = \).*/\120M/' /etc/php/$PHP_VERSION/apache2/php.ini
	sed -i 's/\(^upload_max_filesize = \).*/\120M/' /etc/php/$PHP_VERSION/cli/php.ini
	printf "%s\n" "Reloading and restarting Apache web server..."
	a2enmod rewrite
	systemctl restart apache2
	PUBLIC_IP=`dig +short myip.opendns.com @resolver1.opendns.com`
	printf "Public IP address is: %s\n" "$PUBLIC_IP"
	hostname -I
	printf "%s\n" "FreePBX has been installed and can be accessed at http://IP-ADDRESS/admin"
	printf "%s\n" "Use the appropriate IP address for your system, as indicated above."
	printf "%s\n" "You will need to navigate to this page to complete setup."
	printf "%s\n" "If this system is Internet facing, you are urged to secure your system"
	printf "%s\n" "by restricting access to specific IP addresses only."
}

uninstall_freepbx() {
	printf "%s\n" "Uninstalling and removing FreePBX-specific stuff"
	# ./install_amp --uninstall # where is this file even located?
	rm /usr/sbin/amportal
	# rm -rf /var/lib/asterisk/bin/* # there could be other stuff in here
	rm -rf /var/www/html/admin/*
	rm /etc/amportal.conf
	rm $AST_CONFIG_DIR/amportal.conf
	rm /etc/freepbx.conf
	rm $AST_CONFIG_DIR/freepbx.conf
}

run_testsuite_test() {
	testcount=$(($testcount + 1))
	./runInVenv.sh python3 runtests.py --test=tests/$1
	# XXX It also seems that if pre-reqs are not satisfied, exit code is 0?
	if [ $? -ne 0 ]; then # test failed
		printf "Exit code was %d\n" $?
		ls
		lastrun=`get_newest_astdir` # get the directory containing the logs from the most recently run test
		if [ "$lastrun" = "" ]; then
			echoerr "No test executions found in $AST_SOURCE_PARENT_DIR/testsuite/logs/$1/"
			ls -la $AST_SOURCE_PARENT_DIR/testsuite/logs/$1/
			exit 1
		fi
		ls "$lastrun/ast1/var/log/asterisk/full.txt"
		if [ -f "$lastrun/ast1/var/log/asterisk/full.txt" ]; then
			grep -B 12 "UserEvent(" $lastrun/ast1/var/log/asterisk/full.txt | grep -v "pbx.c: Launching" | grep -v "stasis.c: Topic" # this should provide a good idea of what failed (or at least, what didn't succeed)
		fi
	else # test succeeded
		testsuccess=$(($testsuccess + 1))
	fi
}

run_testsuite_test_only() { # $2 = stress test
	cd $AST_SOURCE_PARENT_DIR/testsuite # required, full paths are not sufficient, we must cd into the dir...
	if [ $? -ne 0 ]; then
		exit 2
	fi
	if [ ! -d "$AST_SOURCE_PARENT_DIR/testsuite/tests/$1" ]; then
		echoerr "No such directory: $AST_SOURCE_PARENT_DIR/testsuite/tests/$1"
		exit 2
	fi
	echo "$AST_SOURCE_PARENT_DIR/testsuite/runtests.py --test=tests/$1"
	iterations=1
	if [ "$2" = "1" ]; then
		iterations=7
	fi
	while [ $iterations -gt 0 ]; do # POSIX for loop
		./runInVenv.sh python3 $AST_SOURCE_PARENT_DIR/testsuite/runtests.py --test=tests/$1
		if [ $? -ne 0 ]; then # test failed
			echo "ls -d -v $AST_SOURCE_PARENT_DIR/testsuite/logs/$1/* | tail -1"
			lastrun=`ls -d -v $AST_SOURCE_PARENT_DIR/testsuite/logs/$1/* | tail -1` # get the directory containing the logs from the most recently run test ############# this is not FreeBSD compatible.
			if [ "$lastrun" = "" ]; then
				echoerr "No test executions found in $AST_SOURCE_PARENT_DIR/testsuite/logs/$1/"
				ls -la $AST_SOURCE_PARENT_DIR/testsuite/logs/$1/
				exit 1
			fi
			ls "$lastrun/ast1/var/log/asterisk/full.txt"
			grep -B 12 "UserEvent(" $lastrun/ast1/var/log/asterisk/full.txt | grep -v "pbx.c: Launching" | grep -v "stasis.c: Topic" # this should provide a good idea of what failed (or at least, what didn't succeed)
			exit 1
		fi
		iterations=$(( iterations - 1 ))
	done
	if [ "$2" = "1" ]; then
		printf "Stress test passed: %s\n" "$1"
	fi
}

run_testsuite_tests() {
	testcount=0
	testsuccess=0
	if [ ! -d "$AST_SOURCE_PARENT_DIR/testsuite" ]; then
		echoerr "Directory $AST_SOURCE_PARENT_DIR/testsuite does not exist!"
		exit 1
	fi
	if [ ! -f "$AST_CONFIG_DIR/asterisk.conf" ]; then
		echoerr "$AST_CONFIG_DIR/asterisk.conf not found"
		exit 1
	fi
	ls -la /usr/sbin/asterisk
	cd $AST_SOURCE_PARENT_DIR/testsuite

	# run manually for good measure, and so we get the full output
	install_package "python3.11-venv"
	./setupVenv.sh

	run_testsuite_test "apps/assert"
	run_testsuite_test "apps/dialtone"
	run_testsuite_test "apps/verify"
	run_testsuite_test "funcs/func_dbchan"

	printf "%d of %d test(s) passed\n" $testsuccess $testcount
	if [ $testcount -ne $testsuccess ]; then
		exit 1
	fi
	exit 0
}

install_phreak_testsuite_test() { # $1 = test path
	parent=`dirname "$1"`
	base=`basename "$1"`
	if [ ! -d "testsuite/tests/$parent" ]; then
		echoerr "Directory testsuite/tests/$parent does not exist!"
		exit 2
	fi
	if [ ! -f "testsuite/tests/$parent/tests.yaml" ]; then
		echoerr "File tests/$parent/tests.yaml does not exist!"
		exit 2
	fi
	if [ ! -d "phreakscript/testsuite/tests/$1" ]; then
		echoerr "Directory phreakscript/testsuite/tests/$1 does not exist!"
		ls -la phreakscript/testsuite/tests
		exit 2
	fi
	cp -r "phreakscript/testsuite/tests/$1" "testsuite/tests/$parent"
	echo "    - test: '$base'" >> "testsuite/tests/$parent/tests.yaml" # rather than have a patch for this file, which could be subject to frequent change and result in a finicky patch that's likely to fail, simply append it to the list of tests to run
	printf "Installed test: %s\n" "$1"
}

add_phreak_testsuite() {
	printf "%s\n" "Applying PhreakNet test suite additions"
	cd $AST_SOURCE_PARENT_DIR
	if [ ! -d phreakscript ]; then # if dir exists, assume it's the repo
		git clone https://github.com/InterLinked1/phreakscript.git
	fi
	cd phreakscript
	if [ $? -ne 0 ]; then
		echoerr "Failed to find phreakscript directory"
		pwd
		exit 1
	fi
	git config pull.rebase false # this is the default. Do this solely to avoid those annoying "Pulling without specifying" warnings...
	git pull # in case it already existed, update the repo
	cd $AST_SOURCE_PARENT_DIR

	install_phreak_testsuite_test "apps/assert"
	install_phreak_testsuite_test "apps/dialtone"
	install_phreak_testsuite_test "apps/verify"
	install_phreak_testsuite_test "funcs/func_dbchan"

	printf "%s\n" "Finished patching testsuite"
}

install_testsuite_itself() {
	apt-get clean
	apt-get update -y
	apt-get upgrade -y

	cd $AST_SOURCE_PARENT_DIR
	if [ -d "testsuite" ]; then
		if [ "$FORCE_INSTALL" = "1" ]; then
			printf "%s\n" "Reinstalling testsuite..."
			rm -rf testsuite
		else
			echoerr "Test Suite already exists... specify --force flag to reinstall"
		fi
	fi
	git clone https://github.com/asterisk/testsuite.git
	if [ $? -ne 0 ]; then
		echoerr "Failed to download testsuite..."
		return 1
	fi
	cd testsuite

	# Apply patch if needed to fix SIPP compilation and python3 detection
	grep "3.5.2" contrib/scripts/install_prereq
	$WGET https://patch-diff.githubusercontent.com/raw/asterisk/testsuite/pull/48.patch
	git apply 48.patch

	./contrib/scripts/install_prereq install
	if [ $? -ne 0 ]; then
		die "Failed to install test suite"
	fi

	# In theory, the below is not necessary:
	pip3 install pyyaml
	pip3 install twisted

	cd $AST_SOURCE_PARENT_DIR
	cd testsuite
	./setupVenv.sh

	if [ $? -ne 0 ]; then
		# Test suite failed to compile.
		# Disable yappcap and try again. This is an old library that no longer compiles on newer systems, e.g. Ubuntu 24.04
		echoerr "Test suite failed to install, disabling yappcap and trying again..."
		sed -i 's|^https://github.com/asterisk/yappcap|#https://github.com/asterisk/yappcap|' extras.txt
		./setupVenv.sh # Retry
	fi

	# ./runInVenv.sh python3 ./runtests.py -t tests/channels/iax2/basic-call/ # run a single basic test
	#./runInVenv.sh python3 ./runtests.py -l # list all tests

	if [ ! -f "$AST_SOURCE_PARENT_DIR/testsuite/tests/apps/tests.yaml" ]; then
		echoerr "tests/apps/tests.yaml doesn't exist?"
		exit 1
	else
		# fix testsuite regression caused by c930bfec37118e37ff271bf381825408d2409fec (missing newline at EOF)
		echo "" >> "$AST_SOURCE_PARENT_DIR/testsuite/tests/apps/tests.yaml"
	fi

	add_phreak_testsuite
	printf "%s\n" "Asterisk Test Suite installation complete"
}

configure_devmode() {
	./configure --enable-dev-mode --with-jansson-bundled --with-pjproject-bundled
	if [ $? -ne 0 ]; then
		exit 2
	fi
	$AST_MAKE menuselect.makeopts
	menuselect/menuselect --enable DONT_OPTIMIZE --enable BETTER_BACKTRACES --enable COMPILE_DOUBLE --enable DO_CRASH menuselect.makeopts
	menuselect/menuselect --enable DEBUG_THREADS --enable MALLOC_DEBUG --enable DEBUG_FD_LEAKS menuselect.makeopts
	menuselect/menuselect --enable TEST_FRAMEWORK --enable-category MENUSELECT_TESTS menuselect.makeopts
}

install_testsuite() { # $1 = $FORCE_INSTALL
	cd $AST_SOURCE_PARENT_DIR
	# in case we're not already in the right directory
	AST_SRC_DIR=`ls -d */ | grep "^asterisk-" | tail -1`
	if [ "$AST_SRC_DIR" = "" ]; then
		echoerr "Asterisk source not found. Aborting..."
		pwd
		ls -la
		return 1
	fi
	cd $AST_SOURCE_PARENT_DIR/$AST_SRC_DIR
	# Ensure we're compiled with dev mode, or recompile in dev mode if needed
	configure_devmode
	$AST_MAKE
	$AST_MAKE install
	install_testsuite_itself
}

install_samples() {
	# Only install sample config files if this is the first time (configs aren't already present on the system)
	if [ ! -f "$AST_CONFIG_DIR/asterisk.conf" ]; then
		if [ -d "$AST_CONFIG_DIR" ]; then
			printf "$AST_CONFIG_DIR existed but not $AST_CONFIG_DIR/asterisk.conf... Installing sample configs now."
		fi
		# the phoneprov files will fail to install if make install has not been run yet, so we do this AFTER make install.
		$AST_MAKE samples # do not run this on upgrades or it will wipe out your config!
		rm $AST_CONFIG_DIR/users.conf # remove users.conf, because a) it's stupid, and shouldn't be used, and b) it creates warnings when reloading other modules, e.g. chan_dahdi.
		# Change a few defaults for our sanity. This makes Asterisk more usable immediately without modifying or replacing the configs.
		sed -i 's/;verbose =/verbose =/' $AST_CONFIG_DIR/asterisk.conf
		sed -i 's/;timestamp =/timestamp =/' $AST_CONFIG_DIR/asterisk.conf
	else
		printf "Skipping sample config installation, since %s already exists\n" $AST_CONFIG_DIR
	fi
}

dahdi_undo() {
	printf "Restoring drivers by undoing PATCH: %s\n" "$3"
	wget -q "https://github.com/asterisk/dahdi-linux/commit/$4.patch" -O /tmp/$2.patch --no-cache
	patch -u -b -p 1 --reverse -i /tmp/$2.patch
	if [ $? -ne 0 ]; then
		echoerr "Failed to reverse DAHDI patch... this should be reported..."
		exit 2
	fi
	rm /tmp/$2.patch
}

dahdi_undo_force() {
	printf "Restoring drivers by undoing PATCH: %s\n" "$3"
	wget -q "https://github.com/asterisk/dahdi-linux/commit/$4.patch" -O /tmp/$2.patch --no-cache
	patch -u -b -p 1 --reverse -i /tmp/$2.patch
	rm /tmp/$2.patch
}

dahdi_custom_undo() {
	printf "Applying custom reverse DAHDI patch: %s\n" "$3"
	wget -q "$4" -O /tmp/$2.patch --no-cache
	patch -u -p 1 --reverse -i /tmp/$2.patch
	if [ $? -ne 0 ]; then
		echoerr "Failed to reverse custom DAHDI patch... this should be reported..."
		exit 2
	fi
	rm /tmp/$2.patch
}

dahdi_custom_patch() {
	printf "Applying custom DAHDI patch: %s\n" "$1"
	if [ ! -f "$AST_SOURCE_PARENT_DIR/$2" ]; then
		echoerr "File $AST_SOURCE_PARENT_DIR/$2 does not exist"
		exit 2
	fi
	wget -q "$3" -O /tmp/$1.patch --no-cache
	patch -u -b "$AST_SOURCE_PARENT_DIR/$2" -i /tmp/$1.patch
	if [ $? -ne 0 ]; then
		echoerr "Failed to apply DAHDI patch (patch -u -b $AST_SOURCE_PARENT_DIR/$2 -i /tmp/$1.patch)... this should be reported..."
		exit 2
	fi
	rm /tmp/$1.patch
}

custom_patch() {
	printf "Applying custom patch: %s\n" "$1"
	if [ ! -f "$2" ]; then
		echoerr "File $2 does not exist"
		exit 2
	fi
	wget -q "$3" -O /tmp/$1.patch --no-cache
	patch -u -b "$2" -i /tmp/$1.patch
	if [ $? -ne 0 ]; then
		echoerr "Failed to apply custom patch (patch -u -b $2 -i /tmp/$1.patch)... this should be reported..."
		exit 2
	fi
	rm /tmp/$1.patch
}

dahdi_patch() {
	printf "Applying unmerged DAHDI patch: %s\n" "$1"
	wget -q "https://github.com/asterisk/dahdi-linux/commit/$1.patch" -O /tmp/$1.patch --no-cache
	patch -u -b -p 1 -i /tmp/$1.patch
	if [ $? -ne 0 ]; then
		echoerr "Failed to apply DAHDI patch... this should be reported..."
		exit 2
	fi
	rm /tmp/$1.patch
}

git_patch() {
	printf "Applying git patch: %s\n" "$1"
	cp "$GIT_REPO_PATH/patches/$1" "/tmp/$1"
	if [ $? -ne 0 ]; then
		die "File $1 does not exist"
	fi
	git apply "/tmp/$1"
	if [ $? -ne 0 ]; then
		die "Failed to apply git patch $1... this should be reported..."
	fi
	rm "/tmp/$1"
}

github_pr() {
	printf "Applying GitHub pull request for %s\n" "$1"
	wget -q "$2" -O /tmp/$1.pr.diff --no-cache
	git apply "/tmp/$1.pr.diff"
	if [ $? -ne 0 ]; then
		echoerr "Failed to apply GitHub PR ($1)... this should be reported..."
		if [ "$3" != "try" ]; then
			exit 2
		else
			# This could be something that needs to be looked into, or maybe this was merged and is no longer relevant.
			printf "Will continue momentarily...\n"
			sleep 10
		fi
	fi
	rm "/tmp/$1.pr.diff"
}

# $2 = 1 to force
asterisk_pr() {
	PR_PATCH_FILE=/tmp/$1.pr.diff
	wget -q "https://patch-diff.githubusercontent.com/raw/asterisk/asterisk/pull/$1.diff" -O $PR_PATCH_FILE --no-cache
	if [ $? -ne 0 ]; then
		echoerr "Failed to download https://patch-diff.githubusercontent.com/raw/asterisk/asterisk/pull/$1.diff"
	fi
	if [ "$2" = "1" ]; then
		git apply -v $PR_PATCH_FILE
		if [ $? -ne 0 ]; then
			echoerr "Failed to apply patch using git apply, retrying directly using patch..."
			patch -p1 -F 3 -f --verbose < $PR_PATCH_FILE
		fi
	else
		git apply $PR_PATCH_FILE
	fi
	if [ $? -ne 0 ]; then
		echoerr "Failed to apply Asterisk PR... this should be reported..."
		cat $PR_PATCH_FILE
		exit 2
	fi
	rm $PR_PATCH_FILE
}

git_custom_patch() {
	CUSTOM_PATCH_FILE=/tmp/tmp_git_patch.diff
	printf "Applying git patch: %s\n" "$1"
	wget -q "$1" -O $CUSTOM_PATCH_FILE --no-cache
	if [ ! -f $CUSTOM_PATCH_FILE ]; then
		die "Failed to download patch $1"
	fi
	git apply $CUSTOM_PATCH_FILE
	if [ $? -ne 0 ]; then
		die "Failed to apply git patch $1... this should be reported..."
	fi
	rm $CUSTOM_PATCH_FILE
}

dahdi_unpurge() { # undo "great purge" of 2018: $1 = DAHDI_LIN_SRC_DIR
	printf "%s\n" "Reverting patches that removed driver support in DAHDI..."
	dahdi_custom_undo $1 "dahdi_pci_module" "Remove unnecessary dahdi_pci_module macro" "https://raw.githubusercontent.com/InterLinked1/phreakscript/master/patches/dahdi_pci_module.diff"
	dahdi_custom_undo $1 "dahdi_irq_handler" "Remove unnecessary DAHDI_IRQ_HANDLER macro" "https://raw.githubusercontent.com/InterLinked1/phreakscript/master/patches/dahdi_irq_handler.diff"
	dahdi_undo $1 "devtype" "Remove struct devtype for unsupported drivers" "75620dd9ef6ac746745a1ecab4ef925a5b9e2988"
	dahdi_undo $1 "wcb" "Remove support for all but wcb41xp wcb43xp and wcb23xp." "29cb229cd3f1d252872b7f1924b6e3be941f7ad3"
	dahdi_undo $1 "wctdm" "Remove support for wctdm800, wcaex800, wctdm410, wcaex410." "a66e88e666229092a96d54e5873d4b3ae79b1ce3"

	# The tor2 and pciradio patches do not revert cleanly on their own. We need to finish it off manually with additional patches:
	dahdi_undo_force $1 "tor2" "Remove support for tor2." "60d058cc7a064b6e07889f76dd9514059c303e0f"
	dahdi_custom_patch "tor2_Kbuild" "$1/drivers/dahdi/Kbuild" "https://raw.githubusercontent.com/InterLinked1/phreakscript/master/patches/tor2_Kbuild.diff"
	dahdi_undo_force $1 "pciradio" "Remove support for pciradio." "bfdfc4728c033381656b59bf83aa37187b5dfca8"
	dahdi_custom_patch "pciradio_Kbuild" "$1/drivers/dahdi/Kbuild" "https://raw.githubusercontent.com/InterLinked1/phreakscript/master/patches/pciradio_Kbuild.diff"

	printf "%s\n" "Finished undoing DAHDI removals!"
}

linux_headers_info() {
	printf "Your kernel: "
	uname -r
	printf "Available headers for your system: \n"
	printf "You may need to upgrade your kernel using apt-get dist-upgrade to install the build headers for your system.\n"
	if [ "$PAC_MAN" = "apt-get" ]; then
		apt search linux-headers 2>1 | grep "linux-headers"
	fi
	printf "Installed:\n"
	ls -la /lib/modules/
}

linux_headers_install_apt() {
	# Special case for Raspberry Pi
	grep -i Model /proc/cpuinfo | grep -q Raspberry
	if [ $? -ne 0 ]; then
		apt-get install -y linux-headers-`uname -r`
	else
		apt-get install -y raspberrypi-kernel-headers
	fi
	if [ $? -ne 0 ]; then
		kernel=`uname -r`
		# Can happen if a newer kernel was installed but hasn't taken effect yet
		echoerr "Unable to find automatic installation candidate for $kernel - reboot needed?"
		# install the generic kernel headers
		# this usually won't happen in a VM, but can happen in containers. For example, the containers used for GitHub actions.
		GENERIC_HEADERS=1
		apt-get install -y linux-headers-amd64
		if [ $? -ne 0 ]; then
			echoerr "Failed to download system headers and exception failed"
			linux_headers_info
			exit 2
		fi
	fi
}

install_wanpipe() {
	MYSOURCEDIR=/lib/modules/$(uname -r)/build
	MYSOURCEDIRORIG=$MYSOURCEDIR
	MYINCLUDEDIR=/usr/src/linux-headers-$(uname -r)/include

	ensure_installed flex # required for wancfg
	ensure_installed bison # required for wanpipe
	ensure_installed libtool # required for libsangoma

	# wanpipe currently fails to install on Debian because the wanpipe Setup.sh doesn't support recursive Makefile includes.
	# Explicitly find the right source directory to use if that's the case.
	# XXX See below: I don't think this is fully correct at the moment, since SOURCEDIR in wanpipe's Setup.sh is used for multiple, unrelated things.
	while true; do
		printf "Checking file: %s\n" "$MYSOURCEDIR/Makefile" >&2
		if [ ! -f "$MYSOURCEDIR/Makefile" ]; then
			echoerr "File $MYSOURCEDIR/Makefile does not exist\n"
			break
		fi

		# POSIX sh doesn't support ${var:x:y} syntax
		contents=$( cat "$MYSOURCEDIR/Makefile" )
		first7=$( echo "$contents" | cut -d" " -f1 )
		if [ "${first7}" = "include" ]; then
			nextfile=$( echo "$contents" | cut -d" " -f2 )
			printf "Following include to %s\n" "${nextfile}" >&2
			MYSOURCEDIR=`dirname "${nextfile}"`
		else
			printf "Actual makefile is %s\n" "$MYSOURCEDIR/Makefile" >&2
			break
		fi
	done

	cd $AST_SOURCE_PARENT_DIR
	# Sangoma clearly doesn't know how to run a web server, since their server keeps going down.
	# Pull tarball from Wayback Machine if Sangoma's is unresponsive
	wget --tries=1 https://ftp.sangoma.com/linux/current_wanpipe/${WANPIPE_SOURCE_NAME}.tgz
	if [ $? -ne 0 ]; then
		# Note: This link needs to be updated for new versions of wanpipe
		wget https://web.archive.org/web/20240708051349/https://ftp.sangoma.com/linux/current_wanpipe/${WANPIPE_SOURCE_NAME}.tgz
	fi
	tar xvfz ${WANPIPE_SOURCE_NAME}.tgz
	WANPIPE_DIR=`tar -tzf $WANPIPE_SOURCE_NAME.tgz | head -1 | cut -f1 -d"/"`
	cd ${WANPIPE_DIR}
	if [ $? -ne 0 ]; then
		die "Failed to download/extract wanpipe"
	fi
	rm ${AST_SOURCE_PARENT_DIR}/${WANPIPE_SOURCE_NAME}.tgz
	#phreak_fuzzy_patch "af_wanpipe.diff"

	#./Setup dahdi --silent
	./Setup install --silent # Even if Setup dahdi fails, this can work

	if [ $? -ne 0 ]; then
		echoerr "wanpipe install failed: unsupported kernel?"
		# No need to cat setup_drv_compile.log here, Setup automatically does so on failure
		# XXX wanpipe currently fails to compile on kernels newer than 6.1.0. See [PHREAKSCRIPT-49]
		if [ "$IGNORE_FAILURES" = "1" ]; then
			printf "Installation of other items will proceed anyways...\n"
			sleep 1
		else
			exit 1
		fi
	else
		wanrouter stop
		wanrouter start
	fi
}

kernel_header_debug() {
	printf "Running kernel: %s\n" $( uname -r )
	printf "KVERS: %s\n" "$KVERS"
	printf "KSRC: %s\n" "$KSRC"
	ls -la /usr/src/
	ls -la /usr/src/kernels
	printf "Available kernel packages:\n"
	dnf search kernel-devel
	printf "Installed kernels:\n"
	rpm -qa | grep kernel
	ls -la /lib/modules
	ls -la /usr/src/
	ls -la /usr/src/kernels
	ls -la /usr/src/kernels/*
}

install_dahdi() {
	# Install the kernel headers
	if [ "$KSRC" = "" ]; then
		printf "Actual kernel version: %s\n" $( uname -r )
		if [ "$PAC_MAN" = "apt-get" ]; then
			install_package "kmod binutils-dev dh-autoreconf libusb-dev"
			linux_headers_install_apt
		elif [ "$PAC_MAN" = "yum" ]; then
			install_package "kmod kernel-devel kernel-headers"
			dnf list installed 'kernel*'
			# Fedora-based systems seem to have a newer kernel-devel present than the actual running kernel.
			# This includes Red Hat, which may install newer headers than the current system: https://access.redhat.com/discussions/4656371?tour=8
			# Try to detect that and fix it.
			KERNEL_DEVEL_VERSION=$( dnf list installed 'kernel-devel' | grep "kernel-devel" | xargs | cut -d' ' -f2 | cut -d'.' -f1-5 )
			kernel_ver=$( uname -r | cut -d'.' -f1-5 )
			if [ "$KERNEL_DEVEL_VERSION" != "$kernel_ver" ]; then
				echoerr "kernel-devel mismatch has been detected. Package provides $KERNEL_DEVEL_VERSION, but running kernel is $kernel_ver"
				echoerr "Installing specific kernel-devel package to match..."
				dnf install -y kernel-devel-$(uname -r)
				KERNEL_DEVEL_VERSION=$( dnf list installed 'kernel-devel' | grep "kernel-devel" | xargs | cut -d' ' -f2 | cut -d'.' -f1-5 )
				if [ "$KERNEL_DEVEL_VERSION" != "$kernel_ver" ]; then
					KERNEL_MMP_VERSION=$( uname -r | cut -d'-' -f1-2 )
					printf "Attempting to install: %s\n" "$KERNEL_MMP_VERSION"
					dnf install -y kernel-devel-$KERNEL_MMP_VERSION
					KERNEL_DEVEL_VERSION=$( dnf list installed 'kernel-devel' | grep "kernel-devel" | xargs | cut -d' ' -f2 | cut -d'.' -f1-5 )
				fi
				if [ "$KERNEL_DEVEL_VERSION" != "$kernel_ver" ] && [ "$RHEL_MAJOR_VERSION_8" != "1" ]; then
					dnf remove -y kernel-devel
					dnf install -y kernel-devel-matched
					KERNEL_DEVEL_VERSION=$( dnf list installed 'kernel-devel' | grep "kernel-devel" | xargs | cut -d' ' -f2 | cut -d'.' -f1-5 )
				fi
				if [ "$KERNEL_DEVEL_VERSION" != "$kernel_ver" ]; then
					echoerr "kernel-devel mismatch still present? ($KERNEL_DEVEL_VERSION != $kernel_ver)"
					if [ "$AUTOSET_KVERS" = "1" ]; then
						printf "Auto-setting KVERS=%s\n" "$KERNEL_DEVEL_VERSION"
						KVERS="$KERNEL_DEVEL_VERSION"
					fi
					if [ "$KVERS" != "" ]; then
						# Kernel version override for GitHub CI builds, where the available headers on Fedora-based distros
						# do not match the running kernel. This probably would not run successfully, but in this case,
						# we just care about building it.
						ls /usr/src/kernels | grep "$KVERS"
						if [ $? -ne 0 ]; then
							printf "Installed kernels:\n"
							ls /usr/src/kernels
							die "No kernel matching specified kernel version, unable to autodetermine KSRC from specifiers KVERS: $KVERS"
						fi
						ksrc_dir=$( ls /usr/src/kernels | grep "${KVERS}" | head -n 1 | tr -d '\n' )
						printf "Kernel source dir: %s\n" "$ksrc_dir"
						new_ksrc="/usr/src/kernels/${ksrc_dir}"
						if [ "$new_ksrc" == "" ]; then
							printf "Installed kernels:\n"
							ls /usr/src/kernels
							die "Couldn't autodetermine KSRC from KVERS"
						fi
						printf "Setting KSRC to %s\n" "$new_ksrc"
						export KSRC="$new_ksrc"
					elif [ "$GENERIC_HEADERS" != "1" ]; then
						kernel_header_debug
						exit 1
					fi
				fi
			else
				echog "kernel-devel is matched with kernel. Package provides $KERNEL_DEVEL_VERSION, and running kernel is $kernel_ver"
			fi
		elif [ "$PAC_MAN" = "zypper" ]; then
			install_package "kmod kernel-source"
		elif [ "$PAC_MAN" = "pacman" ]; then
			install_package "kmod linux-headers"
		elif [ "$PAC_MAN" = "pkg" ]; then
			echoerr "DAHDI is not supported on FreeBSD! Proceed at your own risk!"
			sleep 2
		else
			echoerr "Unable to install potential DAHDI prerequisites"
			sleep 2
		fi

		# Check that the kernel sources are really present
		# /usr/src/linux-headers-* on Debian
		# /usr/src/kernels on Rocky Linux
		numkernheaders=$( ls /usr/src/linux-headers-* /usr/src/kernels/* 2>/dev/null | wc -w )
		if [ "$numkernheaders" = "0" ]; then
			echoerr "Kernel headers do not appear to be installed... compilation will likely fail"
			sleep 2
		fi
	fi

	if [ "$KSRC" != "" ]; then
		if [ ! -d "$KSRC" ]; then
			die "KSRC directory does not exist: $KSRC"
		elif [ ! -f "$KSRC/.config" ]; then
			die "$KSRC/.config does not exist"
		fi
	fi

	# Avoid "Skipping BTF generation for ... due to unavailability of vmlinux"
	# Worked around by copying vmlinux to another location:
	if [ -f /sys/kernel/btf/vmlinux ]; then
		if [ -d /usr/lib/modules/`uname -r`/build/ ]; then
			cp /sys/kernel/btf/vmlinux /usr/lib/modules/`uname -r`/build/
		else
			printf "Couldn't find kernel modules build dir for kernel %s\n" $( uname -r )
			ls -la /usr/lib/modules
		fi
	else
		echoerr "Couldn't find vmlinux... BTF generation may be skipped during driver install..."
	fi

	# Fix missing resolve_btfids
	# https://github.com/aircrack-ng/rtl8188eus/issues/263#issuecomment-1715699688
	# Fixed by commenting out lines 61-63
	KERNEL_MM=$( uname -r | cut -d'.' -f1-2 )
	printf "Kernel major.minor version is %s\n" "$KERNEL_MM"
	KBUILD_DIR="/usr/lib/linux-kbuild-${KERNEL_MM}"
	if [ -d "$KBUILD_DIR" ]; then
		MODFINAL_FILE="${KBUILD_DIR}/scripts/Makefile.modfinal"
		if [ -f "$MODFINAL_FILE" ]; then
			sed -n 61,63p $MODFINAL_FILE
			sed -n 61,63p $MODFINAL_FILE | grep "CONFIG_DEBUG_INFO_BTF_MODULES"
			if [ $? -eq 0 ]; then
				phreak_tree_patch $MODFINAL_FILE "modfinal.diff"
				sed -n 61,63p $MODFINAL_FILE
			else
				echoerr "Skipping modfinal patch, expected content differs on line 61"
			fi
		else
			echoerr "Could not determine path to Makefile.modfinal"
		fi
	else
		echoerr "Could not determine path to kbuild dir"
	fi

	# Requests to downloads.digium.com usually use IPv4, but sometimes use IPv6
	# This is problematic on GitHub Action Runners, which have IPv6 disabled,
	# and won't let you enable it
	IPV6_DISABLED=0
	if which sysctl > /dev/null; then
		# See https://github.com/asterisk/asterisk-ci-actions/blob/main/SetupUbuntuRunner/action.yml
		printf "IPv6 Disabled: "
		sysctl net.ipv6.conf.all.disable_ipv6
		IPV6_DISABLED=$( sysctl net.ipv6.conf.all.disable_ipv6 | cut -d '=' -f2 | xargs | tr -d '\n' )
	fi

	cd $AST_SOURCE_PARENT_DIR
	# just in case, for some reason, these already existed... don't let them throw everything off:
	rm -f dahdi-linux-current.tar.gz dahdi-tools-current.tar.gz $DAHLIN_SRC_NAME $DAHTOOL_SRC_NAME
	$WGET $DAHLIN_SRC_URL
	if [ ! -f $DAHLIN_SRC_NAME ]; then
		# If this happens, wget is probably not installed
		die "Failed to download $DAHLIN_SRC_URL... is wget installed?"
	fi
	$WGET $DAHTOOL_SRC_URL
	DAHDI_LIN_SRC_DIR=`tar -tzf $DAHLIN_SRC_NAME | head -1 | cut -f1 -d"/"`
	DAHDI_TOOLS_SRC_DIR=`tar -tzf $DAHTOOL_SRC_NAME | head -1 | cut -f1 -d"/"`
	if [ -d "$AST_SOURCE_PARENT_DIR/$DAHDI_LIN_SRC_DIR" ]; then
		if [ "$FORCE_INSTALL" = "1" ]; then
			rm -rf $DAHDI_LIN_SRC_DIR
		else
			ls -la
			die "Directory $DAHDI_LIN_SRC_DIR already exists. Please rename or delete this directory and restart installation or specify the force flag."
		fi
	fi
	if [ -d "$AST_SOURCE_PARENT_DIR/$DAHDI_TOOLS_SRC_DIR" ]; then
		if [ "$FORCE_INSTALL" = "1" ]; then
			rm -rf $DAHDI_TOOLS_SRC_DIR
		else
			ls -la
			die "Directory $DAHDI_TOOLS_SRC_DIR already exists. Please rename or delete this directory and restart installation or specify the force flag."
		fi
	fi
	tar -zxvf $DAHLIN_SRC_NAME && rm $DAHLIN_SRC_NAME
	tar -zxvf $DAHTOOL_SRC_NAME && rm $DAHTOOL_SRC_NAME
	if [ ! -d $AST_SOURCE_PARENT_DIR/$DAHDI_LIN_SRC_DIR ]; then
		printf "Directory not found: %s\n" "$AST_SOURCE_PARENT_DIR/$DAHDI_LIN_SRC_DIR"
		exit 2
	fi

	# DAHDI Linux (generally recommended to install DAHDI Linux and DAHDI Tools separately, as opposed to bundled)
	cd $AST_SOURCE_PARENT_DIR/$DAHDI_LIN_SRC_DIR

	DAHDI_CFLAGS=""
	if [ "$GENERIC_HEADERS" = "1" ]; then
		if [ -f /etc/redhat-release ]; then
			ls /usr/src/kernels
			hdrver=`ls /usr/src/kernels`
		else
			ls -la /lib/modules/
			hdrver=`ls /lib/modules`
		fi
		DAHDI_CFLAGS="KVERS=$hdrver" # overwrite from exact match with uname -r to whatever we happen to have
		printf "We do NOT have an exact kernel match: %s\n" "$hdrver"
	else
		printf "We have an exact kernel match: "
		uname -r
	fi

	if [ "$DAHDI_OLD_DRIVERS" = "1" ]; then
		dahdi_unpurge $DAHDI_LIN_SRC_DIR # for some reason, this needs to be applied before the next branch patches
	fi

	instantiate_repo

	# Compiler fixes for 5.17/5.18:
	if [ $DAHDI_MM_VER -lt 33 ]; then
		phreak_fuzzy_patch "dahdi_pci.diff"
		custom_fuzzy_patch "b6d9b417e1992868549d443efad11e4f1513c9d7.diff" "https://gitea.osmocom.org/retronetworking/dahdi-linux/commit/b6d9b417e1992868549d443efad11e4f1513c9d7.diff"
		custom_fuzzy_patch "09adb59cfe2aff9fc1c18cafb44ae0faf811adca.diff" "https://gitea.osmocom.org/retronetworking/dahdi-linux/commit/09adb59cfe2aff9fc1c18cafb44ae0faf811adca.diff"
		# Compiler fixes for newer kernels (5.15, 5.17, 5.18)
		#git_custom_patch "https://github.com/osmocom/dahdi-linux/commit/09adb59cfe2aff9fc1c18cafb44ae0faf811adca.diff"

		# Compiler fixes for 6.1+
		phreak_fuzzy_patch "dahdi_kern_61.diff"
	fi

	# Merged in master, but not yet in a current release
	git_custom_patch "https://github.com/asterisk/dahdi-linux/commit/d7bbc8a96fe767bc4eee15dd43170f298282a4c3.diff" # RHEL fixes for const struct device *dev
	git_custom_patch "https://github.com/asterisk/dahdi-linux/commit/d932d9fbc8b3559829a76fffcedceb78d1fc1887.diff" # dahdi_spantype fix
	git_custom_patch "https://patch-diff.githubusercontent.com/raw/asterisk/dahdi-linux/pull/57.diff" # PR 57: RHEL build fixes
	git_custom_patch "https://patch-diff.githubusercontent.com/raw/asterisk/dahdi-linux/pull/58.diff" # PR 58: non-RHEL build fixes for older kernels
	git_custom_patch "https://patch-diff.githubusercontent.com/raw/asterisk/dahdi-linux/pull/60.diff" # PR 60: Fix old style declaration error on newer systems
	git_custom_patch "https://patch-diff.githubusercontent.com/raw/asterisk/dahdi-linux/pull/62.diff" # PR 62: Rename MAX to MAX_ATTEMPTS to avoid macro redefinition
	git_custom_patch "https://patch-diff.githubusercontent.com/raw/asterisk/dahdi-linux/pull/64.diff" # PR 64: More struct device to const struct device
	git_custom_patch "https://patch-diff.githubusercontent.com/raw/asterisk/dahdi-linux/pull/66.diff" # PR 66: Add braces around empty if body
	git_custom_patch "https://patch-diff.githubusercontent.com/raw/asterisk/dahdi-linux/pull/69.diff" # PR 69: DEFINE_SEMAPHORE for RHEL
	git_custom_patch "https://patch-diff.githubusercontent.com/raw/asterisk/dahdi-linux/pull/32.patch" # PR 32: xpp: Fix 32-bit builds

	# Fix or skip compilation of the XPP driver for 32-bit
	# OS_ARCH=$( uname -m )
	# printf "Detected architecture: %s\n" "$OS_ARCH"
	#if [ "$OS_ARCH" = "armv7l" ]; then
		# I can't test this build at the moment, so to play it safe, I'm going to keep it disabled in this case,
		# given that the xpp drivers are seldom used and especially unlikely to be used by someone with this architecture.
		# TODO Slightly related, once GitHub allows free arm64 builds, do more testing: https://github.com/orgs/community/discussions/19197
		#echoerr "Skipping compilation of XPP driver for this 32-bit architecture! ($OS_ARCH)"
		#mv $AST_SOURCE_PARENT_DIR/$DAHDI_LIN_SRC_DIR/drivers/dahdi/xpp/Kbuild $AST_SOURCE_PARENT_DIR/$DAHDI_LIN_SRC_DIR/drivers/dahdi/xpp/Bad-Kbuild
	#fi

	KERN_VER_MM=$( uname -r | cut -d. -f1-2 )
	OS_DIST_2=$( printf "$OS_DIST_INFO" | cut -d' ' -f1-2)
	if [ "$KERN_VER_MM" = "4.18" ] && [ "$OS_DIST_2" = "Rocky Linux" ]; then
		# See comments at bottom of: https://github.com/asterisk/dahdi-linux/issues/19
		# We need to remove #if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0) and #endif
		# so that this macro takes effect unconditionally on these platforms (4.18 kernel)
		printf "Detected older kernel that requires the netif_napi_add patch regardless...\n"
		# This only appears once, so it's fine to play fast and loose with sed here...
		# This will trigger a warning, but DAHDI Linux does not use -Werror so this is fine
		# If it did, this path would need to #undef first before the #define
		# /usr/src/dahdi-linux-3.3.0/include/dahdi/kernel.h:62: warning: "netif_napi_add" redefined
		sed -i 's/KERNEL_VERSION(6, 1, 0)/KERNEL_VERSION(4, 18, 0)/g' include/dahdi/kernel.h
	fi

	# New Features
	if [ "$EXTRA_FEATURES" = "1" ]; then
		# Real time dial pulsing (DAHDI to Asterisk)
		git_patch "dahdi_rtoutpulse.diff"
		# Loop disconnect on-demand during a call
		# There are 2 patches here. The first can be applied using git apply
		git_patch "kewl.diff"
		# The 2nd one DOES NOT APPLY with git apply and WILL FAIL, because fuzz (offset 33 lines) and git apply is too stupid to deal with that.
		# Therefore, fallback to using patch, manually, for it.
		phreak_fuzzy_patch "kewl2.diff"
		# hearpulsing
		if [ "$HEARPULSING" = "1" ]; then
			git_patch "hearpulsing-dahlin.diff"
		fi
		if [ "$EMPULSE" = "1" ]; then
			printf "Enabling EMPULSE for this build\n"
			sed -i 's|/\* #define EMPULSE \*/|#define EMPULSE|g' include/dahdi/dahdi_config.h # Enable EMPULSE by default. Note that the * must be escaped for sed.
		fi
	else
		echoerr "Skipping DAHDI Linux feature patches..."
	fi

	# If IPv6 is disabled, force wget downloads to use the IPv4 address
	# Could also use --prefer-family, but is IPv6 is disabled, we have to use IPv4 anyways
	if [ "$IPV6_DISABLED" = "1" ]; then
		echoerr "IPv6 is disabled, forcing driver downloads to use only IPv4..."
		sed -i 's/WGET_ARGS:=--continue/WGET_ARGS:=--inet4-only --continue/g' drivers/dahdi/firmware/Makefile
		grep "WGET" drivers/dahdi/firmware/Makefile
	fi

	if [ "$DAHDI_OLD_DRIVERS" = "1" ]; then
		# For some reason, tor2 won't compile on older kernels due to missing target for "makefw"
		# But if we compile it by its full name first, that takes care of that
		# TODO BUGBUG This is a build order bug that should be addressed in the build system
		$AST_MAKE $AST_SOURCE_PARENT_DIR/$DAHDI_LIN_SRC_DIR/drivers/dahdi/makefw $DAHDI_CFLAGS
	fi

	# if KSRC/KVERS env vars are set, they will automatically propagate to children
	$AST_MAKE -j$(nproc) $DAHDI_CFLAGS
	if [ $? -ne 0 ]; then
		if [ -f drivers/dahdi/vpmadt032_loader/vpmadt032_x86_64.o_shipped ]; then
			# If this is PHREAKSCRIPT-61, apply temporary workaround for newer kernels failing with:
			# No rule to make target 'vpmadt032_loader/vpmadt032_x86_64.o', needed by 'dahdi_vpmadt032_loader.o'.
			#
			# On newer kernels, there seems to be some kind of issue (not sure with what, exactly)
			# where vpmadt032_x86_64.o doesn't exist. On older kernels, it's generated by a COPY:
			# COPY    /usr/src/dahdi-linux-3.4.0/drivers/dahdi/vpmadt032_loader/vpmadt032_x86_64.o
			#
			# This is likely an issue somewhere in the DAHDI Linux build system, possibly drivers/dahdi/Kbuild...
			# But until this issue is fixed, just manually copy the file at this point so compilation can continue.
			ARCH=$( uname -m )
			# This logic is from drivers/dahdi/Kbuild
			if [ "$ARCH" = "i386" ]; then
				MY_DAHDI_ARCH=x86_32
			else
				if [ "$ARCH" = "x86_64" ]; then
					MY_DAHDI_ARCH=x86_64
				else
					grep "CONFIG_X86_32=y" /boot/config-$(uname -r)
					if [ $? -eq 0 ]; then
						MY_DAHDI_ARCH=x86_32
					else
						MY_DAHDI_ARCH=x86_64
					fi
				fi
			fi
			printf "Detected DAHDI_ARCH: '%s'\n" "$MY_DAHDI_ARCH"
			echoerr "Manually performing COPY for 'vpmadt032_${MY_DAHDI_ARCH}.o'"
			printf "COPY %s %s\n" drivers/dahdi/vpmadt032_loader/vpmadt032_${MY_DAHDI_ARCH}.o_shipped drivers/dahdi/vpmadt032_loader/vpmadt032_${MY_DAHDI_ARCH}.o
			cp -n drivers/dahdi/vpmadt032_loader/vpmadt032_${MY_DAHDI_ARCH}.o_shipped drivers/dahdi/vpmadt032_loader/vpmadt032_${MY_DAHDI_ARCH}.o
		fi
		$AST_MAKE -j$(nproc) $DAHDI_CFLAGS
	fi
	if [ $? -ne 0 ]; then
		die "DAHDI Linux compilation failed, aborting install"
	fi
	$AST_MAKE install $DAHDI_CFLAGS

	# DAHDI Tools
	if [ ! -d $AST_SOURCE_PARENT_DIR/$DAHDI_TOOLS_SRC_DIR ]; then
		printf "Directory not found: %s\n" "$AST_SOURCE_PARENT_DIR/$DAHDI_TOOLS_SRC_DIR"
		exit 2
	fi
	cd $AST_SOURCE_PARENT_DIR/$DAHDI_TOOLS_SRC_DIR

	# New Features

	# fix static inline function get_ver (GitHub dahdi-tools #11)
	phreak_fuzzy_patch "dahdi_tools_inline_get_ver.diff"	

	# Not yet merged
	git_custom_patch "https://patch-diff.githubusercontent.com/raw/asterisk/dahdi-tools/pull/22.diff" # PR 22 (dahdi_cfg: fix truncation warning)

	# hearpulsing
	if [ "$EXTRA_FEATURES" = "1" ]; then
		if [ "$HEARPULSING" = "1" ]; then
			git_patch "hearpulsing-dahtool.patch" # hearpulsing
		fi
	fi

	autoreconf -i && [ -f config.status ] || ./configure --with-dahdi=../linux # https://issues.asterisk.org/jira/browse/DAHTOOL-84
	./configure # Both configures are needed!
	$AST_MAKE -j$(nproc) $DAHDI_CFLAGS
	if [ $? -ne 0 ]; then
		die "DAHDI Tools compilation failed, aborting install"
	fi
	$AST_MAKE install
	$AST_MAKE install-config

	if [ -d /usr/include/dahdi ] && [ -f /usr/include/dahdi/user.h ] && [ ! -f /usr/include/dahdi/tonezone.h ]; then
		# For some reason, tonezone.h (which is part of dahdi-tools, not dahdi-linux), ends up not installed
		# in the system headers directory on some systems (e.g. Arch Linux)
		# Just copy it there manually in this case
		echoerr "tonezone.h was not found to be installed in the system headers... manually installing it"
		cp tonezone.h /usr/include/dahdi
	fi

	# Ensure that dahdi_tool is installed, since it's not built if the prereqs weren't available at compile time.
	if ! which "dahdi_tool" > /dev/null; then
		echoerr "dahdi_tool does not appear to have been built successfully... newt development package missing?"
		if [ "$FORCE_INSTALL" != "1" ] && [ "$PAC_MAN" != "pacman" ] && [ "$PAC_MAN" != "yum" ]; then
			# Even though we successfully install the newt dev package on Arch Linux / Fedora 42, for some reason this still fails, so don't make it fatal on those
			exit 1
		fi
	fi

	# All right, here we go...
	dahdi_scan -vvvvv
	if [ ! -f /etc/dahdi/system.conf ]; then
		printf "%s\n" "Generating /etc/dahdi/system.conf for the first time..."
		service dahdi restart
		dahdi_genconf -vvvvv
	else
		echoerr "/etc/dahdi/system.conf already exists, not overwriting..."
		service dahdi restart
	fi
	dahdi_cfg -vvvvv
	dahdi_hardware
	if [ -f /etc/dahdi/modules ]; then
		cat /etc/dahdi/modules
		# modprobe <listed>
	else
		echoerr "No DAHDI modules present"
	fi
	lsdahdi

	# Verify what we did...
	cat /etc/dahdi/system.conf
	if [ $? -ne 0 ]; then
		echo "Uh oh, no /etc/dahdi/system.conf ???"
	fi
	if [ -f /etc/dahdi/assigned-spans.conf ]; then
		cat /etc/dahdi/assigned-spans.conf
	else
		echoerr "No assigned spans"
	fi
	if [ -f $AST_CONFIG_DIR/dahdi-channels.conf ]; then
		cat $AST_CONFIG_DIR/dahdi-channels.conf
	else
		echoerr "No DAHDI channels"
	fi

	# LibPRI # https://gist.github.com/debuggerboy/3028532
	cd $AST_SOURCE_PARENT_DIR
	wget http://downloads.asterisk.org/pub/telephony/libpri/releases/${LIBPRI_SOURCE_NAME}.tar.gz
	tar -zxvf ${LIBPRI_SOURCE_NAME}.tar.gz
	rm ${LIBPRI_SOURCE_NAME}.tar.gz
	cd ${LIBPRI_SOURCE_NAME}
	$AST_MAKE && $AST_MAKE install

	# LibSS7
	cd $AST_SOURCE_PARENT_DIR
	wget https://github.com/asterisk/libss7/archive/refs/tags/${LIBSS7_VERSION}.tar.gz
	tar -zxvf ${LIBSS7_VERSION}.tar.gz
	rm ${LIBSS7_VERSION}.tar.gz
	ls -la
	cd libss7-${LIBSS7_VERSION}
	$AST_MAKE && $AST_MAKE install

	# Wanpipe
	if [ "$DAHDI_WANPIPE" = "1" ]; then
		install_wanpipe
	fi

	service dahdi restart

	# Completion message for users just running "phreaknet dahdi"
	echog "DAHDI and friends have finished installing"
}

phreak_tree_module() { # $1 = file to patch, $2 = whether failure is acceptable
	printf "Adding new module: %s\n" "$1"
	cp "$GIT_REPO_PATH/$1" "$AST_SOURCE_PARENT_DIR/$AST_SRC_DIR/$1"
	if [ $? -ne 0 ]; then
		echoerr "Failed to copy module: $1"
		if [ "$2" != "1" ]; then # unless failure is acceptable, abort
			exit 2
		fi
	fi
}

phreak_tree_module_branch() { # $1 = file to patch, $2 = whether failure is acceptable, $3 = branch name
	printf "Adding new module: %s\n" "$1"
	# Always need to download, since the local copy of the git repo is on the master branch
	wget -q "https://raw.githubusercontent.com/InterLinked1/phreakscript/$3/$1" -O "$AST_SOURCE_PARENT_DIR/$AST_SRC_DIR/$1" --no-cache
	if [ $? -ne 0 ]; then
		echoerr "Failed to download module from branch $3, retrying with master..."
		# Fall back to master in case the branch name is stale. If that fails, abort.
		phreak_tree_module "$1" "$2"
	fi
}

custom_module() { # $1 = filename, $2 = URL to download
	printf "Adding new module: %s\n" "$1"
	wget -q "$2" -O "$AST_SOURCE_PARENT_DIR/$AST_SRC_DIR/$1" --no-cache
	if [ $? -ne 0 ]; then
		echoerr "Failed to download module: $1"
		if [ "$2" != "1" ]; then # unless failure is acceptable, abort
			exit 2
		fi
	fi
}

phreak_nontree_patch() { # $1 = patched file, $2 = patch name
	printf "Applying patch %s to %s\n" "$2" "$1"
	wget -q "$3" -O "/tmp/$2" --no-cache
	if [ $? -ne 0 ]; then
		echoerr "Failed to download patch: $2"
		exit 2
	fi
	patch -u -b "$1" -i "/tmp/$2"
	if [ $? -ne 0 ]; then
		echoerr "Failed to apply patch $2"
		exit 2
	fi
	rm "/tmp/$2"
}

phreak_tree_patch() { # $1 = patched file, $2 = patch name
	printf "Applying patch %s to %s\n" "$2" "$1"
	cp "$GIT_REPO_PATH/patches/$2" "/tmp/$2"
	if [ $? -ne 0 ]; then
		echoerr "Failed to copy patch: $2"
		exit 2
	fi
	patch -u -b "$1" -i "/tmp/$2"
	if [ $? -ne 0 ]; then
		echoerr "Failed to apply patch $2"
		exit 2
	fi
	rm "/tmp/$2"
}

phreak_fuzzy_patch() {
	printf "Applying patch %s to %s\n" "$1" "$1"
	cp "$GIT_REPO_PATH/patches/$1" "/tmp/$1"
	if [ $? -ne 0 ]; then
		echoerr "Failed to download patch: $1"
		exit 2
	fi
	patch -p1 < "/tmp/$1"
	if [ $? -ne 0 ]; then
		echoerr "Failed to apply patch $1"
		ls -la "/tmp/$1"
		exit 2
	fi
	rm "/tmp/$1"
}

custom_fuzzy_patch() {
	printf "Applying patch %s to %s\n" "$1" "$1"
	wget -q "$2" -O "/tmp/$1" --no-cache
	if [ $? -ne 0 ]; then
		echoerr "Failed to download patch: $1"
		exit 2
	fi
	patch -p1 < "/tmp/$1"
	if [ $? -ne 0 ]; then
		echoerr "Failed to apply patch $1"
		ls -la "/tmp/$1"
		exit 2
	fi
	rm "/tmp/$1"
}

# Only for numbered versions, not for master
# $1 = maximum version to match for a branch (non inclusive), e.g. 210100 for 21.1.0
# $2 = PR number
asterisk_pr_if_single() {
	mm_desired=`echo $1 | cut -c 1-2`
	if [ "$1" != "" ]; then
		# This is C-style integer division, which is what we want
		mm=$(( $AST_MM_VER / 10000 ))
		if [ "$mm" = "$mm_desired" ]; then
			if [ $AST_MM_VER -lt $1 ]; then
				printf "Applying PR %d because $AST_MM_VER matches %d and < %s\n" $2 "$mm_desired" "$1"
				asterisk_pr $2 0
			fi
		fi
	fi
}

asterisk_pr_if() {
	# If we're master, patch never applies since we're already merged
	if [ $AST_MM_VER -eq 999999 ]; then
		printf "Skipping patch of PR %d since already merged into master\n" $1
		return
	fi
	asterisk_pr_if_single $2 $1
	asterisk_pr_if_single $3 $1
	asterisk_pr_if_single $4 $1
	if [ "$5" != "" ]; then # At some times of the year, there will be 4 branches actively supported
		asterisk_pr_if_single $5 $1
	fi
}

asterisk_pr_unconditional() {
	printf "Applying PR %d unconditionally\n" $1
	asterisk_pr $1
}

add_experimental() {
	custom_module "include/asterisk/res_pjsip_body_generator_types.h" "https://code.phreaknet.org/asterisk/res_pjsip_body_generator_types.h"
	custom_module "res/res_pjsip_device_features.c" "https://code.phreaknet.org/asterisk/res_pjsip_device_features.c"
	custom_module "res/res_pjsip_device_features_body_generator.c" "https://code.phreaknet.org/asterisk/res_pjsip_device_features_body_generator.c"
	custom_module "res/res_pjsip_sca.c" "https://code.phreaknet.org/asterisk/res_pjsip_sca.c"
	custom_module "res/res_pjsip_sca_body_generator.c" "https://code.phreaknet.org/asterisk/res_pjsip_sca_body_generator.c"
}

# Instantiate an instance of the PhreakScript repository, if not already present
# This is necessary since this script file is designed to be able to be used standalone,
# without the rest of the repository necessarily being present.
instantiate_repo() {
	# GitHub no longers allows svn access (which was useful to download a subset of a repo)
	# Settle for using a shallow clone
	# At this point, this is more efficient than individually downloading all of the files
	# used by phreak_tree_module and phreak_tree_patch.
	if [ "${GIT_REPO_PATH}" = "" ]; then
		# pushd is a bashism, can't use it!
		ORIG_DIR="$PWD"
		cd /tmp
		if [ ! -d phreakscript ]; then
			printf "GIT_REPO_PATH not already set and not found in /tmp, cloning...\n"
			git clone --depth=1 https://github.com/InterLinked1/phreakscript.git
		else
			printf "GIT_REPO_PATH not already set but found in /tmp, updating...\n"
			cd /tmp/phreakscript
			git checkout master
			git pull
		fi
		GIT_REPO_PATH=/tmp/phreakscript
		if [ ! -d $GIT_REPO_PATH ]; then
			die "Directory does not exist: $GIT_REPO_PATH"
		fi
		printf "GIT_REPO_PATH is now %s\n" "$GIT_REPO_PATH"
		cd "$ORIG_DIR"
	else
		printf "GIT_REPO_PATH already provided: %s\n" "$GIT_REPO_PATH"
		if [ ! -d "$GIT_REPO_PATH" ]; then
			die "Directory does not exist: $GIT_REPO_PATH"
		fi
	fi
}

phreak_patches() { # $1 = $PATCH_DIR, $2 = $AST_SRC_DIR
	### Inject custom PhreakNet patches to add additional functionality and features.
	### If/when/as these are integrated upstream, they will be removed from this function. 

	instantiate_repo

	cd $AST_SOURCE_PARENT_DIR/$2

	## Add Standalone PhreakNet Modules
	# XXX In theory, something like cp $GIT_REPO_PATH/apps/*.c apps, etc. would also suffice, rather than enumerating
	phreak_tree_module "apps/app_acts.c"
	phreak_tree_module "apps/app_assert.c"
	phreak_tree_module "apps/app_audichron.c"
	phreak_tree_module "apps/app_callback.c"
	phreak_tree_module "apps/app_callerid.c"
	phreak_tree_module "apps/app_coin.c"
	phreak_tree_module "apps/app_ccsa.c"
	phreak_tree_module "apps/app_dahdimonitor.c"
	phreak_tree_module "apps/app_dialtone.c"
	phreak_tree_module "apps/app_frame.c"
	phreak_tree_module "apps/app_george.c"
	phreak_tree_module "apps/app_keyprefetch.c"
	phreak_tree_module "apps/app_loopplayback.c"
	phreak_tree_module "apps/app_mail.c"
	phreak_tree_module "apps/app_memory.c"
	phreak_tree_module "apps/app_mwi.c"
	phreak_tree_module "apps/app_partialplayback.c"
	phreak_tree_module "apps/app_playdigits.c"
	phreak_tree_module "apps/app_predial.c"
	phreak_tree_module "apps/app_pulsar.c"
	phreak_tree_module "apps/app_randomplayback.c"
	phreak_tree_module "apps/app_remoteaccess.c"
	phreak_tree_module "apps/app_saytelnumber.c"
	phreak_tree_module "apps/app_selective.c"
	phreak_tree_module "apps/app_softmodem.c"
	phreak_tree_module "apps/app_streamsilence.c"
	phreak_tree_module "apps/app_tonetest.c"
	phreak_tree_module "apps/app_verify.c"
	phreak_tree_module "apps/app_wakeupcall.c"
	phreak_tree_module "apps/app_wrappers.c"

	phreak_tree_module "configs/samples/verify.conf.sample" "1" # will fail for obsolete versions of Asterisk b/c of different directory structure, okay.
	phreak_tree_module "configs/samples/irc.conf.sample" "1" # will fail for obsolete versions of Asterisk b/c of different directory structure, okay.
	phreak_tree_module "configs/samples/res_alarmsystem.conf.sample" "1"

	phreak_tree_module "funcs/func_dbchan.c"
	phreak_tree_module "funcs/func_dtmf_flash.c"
	phreak_tree_module "funcs/func_dtmf_trace.c"
	phreak_tree_module "funcs/func_message_sub.c"
	phreak_tree_module "funcs/func_nanpa.c"
	phreak_tree_module "funcs/func_notchfilter.c"
	phreak_tree_module "funcs/func_numpeer.c"
	phreak_tree_module "funcs/func_query.c"
	phreak_tree_module "funcs/func_resonance.c"
	phreak_tree_module "funcs/func_tech.c"

	phreak_tree_module "res/res_coindetect.c"
	phreak_tree_module "res/res_alarmsystem.c"
	phreak_tree_module "res/res_digitmap.c"
	phreak_tree_module "res/res_irc.c"
	phreak_tree_module "res/res_msp.c"
	phreak_tree_module "res/res_pbx_validate.c"
	phreak_tree_module "res/res_phreaknet.c"
	phreak_tree_module "res/res_pjsip_presence.c"
	phreak_tree_module "res/res_smdr_whozz.c"

	if [ "$DEVMODE" = "1" ]; then
		phreak_tree_module "res/res_deadlock.c" # this is not possibly useful to non-developers
	fi
	if [ -d /etc/dahdi ]; then
		phreak_tree_module "apps/app_loopdisconnect.c"
		if [ "$RTPULSING" = "1" ]; then
			phreak_tree_module "res/res_dialpulse.c"
		fi
	fi

	## Third Party Modules
	if [ "$HAVE_COMPATIBLE_SPANDSP" = "1" ]; then
		custom_module "apps/app_tdd.c" "https://raw.githubusercontent.com/dgorski/app_tdd/main/app_tdd.c"
		# TODO: Remove once PR is merged
		#cd apps # Since this repo has app_tdd in the root directory, we need to match it to use git apply
		#github_pr "app_tdd_compiler_fix" "https://patch-diff.githubusercontent.com/raw/dgorski/app_tdd/pull/21.diff" "try"
		#cd ..
	fi
	custom_module "apps/app_fsk.c" "https://raw.githubusercontent.com/alessandrocarminati/app-fsk/master/app_fsk_18.c"
	if [ "$OS_DIST_INFO" = "FreeBSD" ]; then
		sed -i '' '/defaultenabled/d' apps/app_fsk.c
	else
		sed -i 's/<defaultenabled>no<\/defaultenabled>//g' apps/app_fsk.c # temporary bug fix
	fi

	## Add patches to existing modules
	phreak_tree_patch "apps/app_stack.c" "returnif.patch" # Add ReturnIf application
	if [ "$ENHANCED_CHAN_SIP" != "1" ]; then
		phreak_tree_patch "channels/chan_sip.c" "sipfaxcontrol.diff" # chan_sip: Add fax timing controls
	fi
	phreak_tree_patch "main/loader.c" "loader_deprecated.patch" # Don't throw alarmist warnings for deprecated ADSI modules that aren't being removed
	phreak_tree_patch "main/manager_channels.c" "disablenewexten.diff" # Disable Newexten event, which significantly degrades dialplan performance
	phreak_tree_patch "main/dsp.c" "coindsp.patch" # DSP additions
	# Enhanced chan_sip already has this included
	if [ "$ENHANCED_CHAN_SIP" != "1" ] && [ "$SIP_CISCO" != "1" ]; then # XXX this patch has a merge conflict with SIP usecallmanager patches
		git_patch "sipcustparams.patch" # chan_sip: Add custom parameter support, adds SIP_PARAMETER function.
	fi

	if [ "$HEARPULSING" = "1" ]; then
		git_patch "hearpulsing-ast.diff" # DAHDI hearpulsing
	fi

	if [ "$WEAK_TLS" = "1" ]; then
		phreak_tree_patch "res/res_srtp.c" "srtp.diff" # Temper SRTCP unprotect warnings. Only beneficial for older ATAs that require older TLS protocols.
	fi

	printf "Applying patches applicable to %s -> %d (~%s)\n" "$AST_ALT_VER" "$AST_MM_VER" "$AST_MAJOR_VER"

	## merged into master, not yet in a release version (use asterisk_pr_if, e.g. asterisk_pr_if 399 210100 200600 182100)
	asterisk_pr_if 961 220200 210700 201200 182700 # config.c: fix template inheritance/overrides
	asterisk_pr_if 994 220200 210700 201200 182700 # FGD regression fix
	asterisk_pr_if 245 220200 210700 201200 182700 # config.c: fix template inheritance/overrides
	asterisk_pr_if 971 220200 210700 201200 182700 # config.c fix issues w/ whitespace in comments
	asterisk_pr_if 414 220200 210700 201200 182700 # IAX2 loopback warning
	asterisk_pr_if 1030 220200 210700 201200 182700 # chan_dahdi: Fix wrong channel state when RINGING recieved
	asterisk_pr_if 1055 220200 210700 201200 182700 # chan_iax2: Avoid unnecessarily backlogging frames
	#asterisk_pr_if 918 220200 210700 201200 182700 # config.c #tryinclude fixes. Temporarily disabled since patch fails to apply: main/config.c:2750
	#asterisk_pr_if 438 220200 210700 201200 182700 # Last Number Redial. This now conflicts with 272, so temp. disabled.

	## Unmerged patches: remove or switch to asterisk_pr_if once merged
	asterisk_pr_unconditional 1086 # Fix for Fedora 42 (old style definitions for libdb)
	asterisk_pr_unconditional 1089 # app_sms: Ignore false positive gcc warning
	#asterisk_pr_unconditional 272 # Call Waiting Deluxe. This also now conflicts (with the latest revisions), so temp. disabled.
	#asterisk_pr_unconditional 292 # GROUP VARs # Disabled temporarily as patch does not apply anymore
	git_custom_patch "https://code.phreaknet.org/asterisk/dahdicleanup.diff"

	if [ $AST_MAJOR_VER -lt 21 ]; then
		if [ "$EXTERNAL_CODECS" = "1" ]; then
			phreak_nontree_patch "main/translate.c" "translate.diff" "https://issues.asterisk.org/jira/secure/attachment/60464/translate.diff" # Bug fix to translation code
		else
			# WARNING: This will cause a crash due to ABI incompatibility if any external codecs are used. Use the older translate.diff patch in that case.
			# This has been merged into master, but for the above reason will not appear in Asterisk until v21 when new binary codec modules are created for external codecs.
			git_patch "translate_c_Prefer_better_codecs_on_ties.patch" # translate.c: Prefer better codecs on ties.
		fi
	fi

	if [ "$RTPULSING" = "1" ]; then
		# Patches split up to make it easier to selectively redo the 2nd one if a patch conflict occurs and the patch needs to be rebased.
		git_patch "ast_rtoutpulsing1.diff" # chan_dahdi: add rtoutpulsing
		git_patch "ast_rtoutpulsing2.diff" # chan_dahdi: add rtoutpulsing
	fi

	# Out of tree patches
	git_patch "app_confbridge_Fix_bridge_shutdown_race_condition.patch" # app_confbridge: Fix bridge shutdown race condition
	git_patch "blueboxing.diff" # dsp: make blue boxing easier
	git_patch "prefixinclude.diff" # pbx: prefix includes
	git_patch "agi_record_noisefirst.diff" # res_agi: Add noise before silence detection option to Record AGI
	git_patch "19655-asterisk.c-Prevent-duplicate-Asterisk-processes-fro.patch" # Prevent duplicate Asterisk process creation

	if [ "$EXPERIMENTAL_FEATURES" = "1" ] && [ $AST_MAJOR_VER -ge 21 ]; then
		printf "Installing 21+ patches for experimental features\n"
		add_experimental
	fi
}

freebsd_port_patch() {
	filename=$( basename $1 )
	$WGET "$1" -O "$filename" --no-cache
	affectedfile=$( head -n 2 $filename | tail -n 1 | cut -d' ' -f2 )
	if [ ! -f "$affectedfile" ]; then
		echoerr "File does not exist: $affectedfile (patched by $filename)"
		exit 2
	fi
	patch $affectedfile -i $filename
	rm "$filename"
}

freebsd_port_patches() { # https://github.com/freebsd/freebsd-ports/tree/7abe6caf38ac7e552642372be9b4136c4354d0fd/net/asterisk18/files
	# todo: some of these are obsolete and some can be integrated upstream
	printf "%s\n" "Applying FreeBSD Port Patches..."
	pwd
	freebsd_port_patch "https://raw.githubusercontent.com/freebsd/freebsd-ports/7abe6caf38ac7e552642372be9b4136c4354d0fd/net/asterisk18/files/patch-Makefile"
	freebsd_port_patch "https://raw.githubusercontent.com/freebsd/freebsd-ports/7abe6caf38ac7e552642372be9b4136c4354d0fd/net/asterisk18/files/patch-agi_Makefile"
	freebsd_port_patch "https://raw.githubusercontent.com/freebsd/freebsd-ports/7abe6caf38ac7e552642372be9b4136c4354d0fd/net/asterisk18/files/patch-channels_chan__dahdi.c"
	if [ "$CHAN_SIP" = "1" ]; then
		freebsd_port_patch "https://raw.githubusercontent.com/freebsd/freebsd-ports/7abe6caf38ac7e552642372be9b4136c4354d0fd/net/asterisk18/files/patch-channels_sip_include_sip.h"
	fi
	freebsd_port_patch "https://raw.githubusercontent.com/freebsd/freebsd-ports/7abe6caf38ac7e552642372be9b4136c4354d0fd/net/asterisk18/files/patch-configure"
	freebsd_port_patch "https://raw.githubusercontent.com/freebsd/freebsd-ports/7abe6caf38ac7e552642372be9b4136c4354d0fd/net/asterisk18/files/patch-contrib_Makefile"
	freebsd_port_patch "https://raw.githubusercontent.com/freebsd/freebsd-ports/7abe6caf38ac7e552642372be9b4136c4354d0fd/net/asterisk18/files/patch-main_Makefile"
	freebsd_port_patch "https://raw.githubusercontent.com/freebsd/freebsd-ports/7abe6caf38ac7e552642372be9b4136c4354d0fd/net/asterisk18/files/patch-main_asterisk.exports.in"
	freebsd_port_patch "https://raw.githubusercontent.com/freebsd/freebsd-ports/7abe6caf38ac7e552642372be9b4136c4354d0fd/net/asterisk18/files/patch-main_http.c"
	freebsd_port_patch "https://raw.githubusercontent.com/freebsd/freebsd-ports/7abe6caf38ac7e552642372be9b4136c4354d0fd/net/asterisk18/files/patch-main_lock.c"
	freebsd_port_patch "https://raw.githubusercontent.com/freebsd/freebsd-ports/7abe6caf38ac7e552642372be9b4136c4354d0fd/net/asterisk18/files/patch-third-party_pjproject_Makefile"
	#freebsd_port_patch "https://raw.githubusercontent.com/freebsd/freebsd-ports/7abe6caf38ac7e552642372be9b4136c4354d0fd/net/asterisk18/files/patch-third-party_pjproject_Makefile.rules"
	printf "%s\n" "FreeBSD patching complete..."
}

install_odbc() {
	# https://wiki.asterisk.org/wiki/display/AST/Getting+Asterisk+Connected+to+MySQL+via+ODBC
	if [ "$PAC_MAN" = "apt-get" ]; then
		# unixodbc is what contains isql
		install_package "unixodbc unixodbc-dev odbcinst"
	fi
	cd $AST_SOURCE_PARENT_DIR
	# http://www.unixodbc.org/ to get isql (if desired for manual testing)
	# https://mariadb.com/downloads/#connectors
	# |-> https://downloads.mariadb.com/Connectors/odbc/connector-odbc-3.1.11/
	# |---> https://downloads.mariadb.com/Connectors/odbc/connector-odbc-3.1.11/mariadb-connector-odbc-3.1.11-debian-buster-amd64.tar.gz
	wget https://downloads.mariadb.com/Connectors/odbc/connector-odbc-$ODBC_VER/mariadb-connector-odbc-$ODBC_VER-debian-buster-amd64.tar.gz
	if [ $? -ne 0 ]; then
		die "MariaDB ODBC download failed"
	fi
	tar -zxvf mariadb-connector-odbc-$ODBC_VER-debian-buster-amd64.tar.gz
	cd mariadb-connector-odbc-$ODBC_VER-debian-buster-amd64/lib/mariadb
	if [ ! -d /usr/lib/x86_64-linux-gnu/odbc/ ]; then
		mkdir /usr/lib/x86_64-linux-gnu/odbc/
	fi
	cp libmaodbc.so /usr/lib/x86_64-linux-gnu/odbc/
	cp libmariadb.so /usr/lib/x86_64-linux-gnu/odbc/
	cd $AST_SOURCE_PARENT_DIR
	rm mariadb-connector-odbc-$ODBC_VER-debian-buster-amd64.tar.gz
	odbcinst -j
	if [ $? -ne 0 ]; then
		die "odbcinst failed"
	fi
	echo "[MariaDB]" > mariadb.ini
	echo "Description	= Maria DB" >> mariadb.ini
	echo "Driver = /usr/lib/x86_64-linux-gnu/odbc/libmaodbc.so" >> mariadb.ini
	echo "Setup = /usr/lib/x86_64-linux-gnu/odbc/libodbcmyS.so" >> mariadb.ini
	odbcinst -i -d -f mariadb.ini
	rm mariadb.ini
	# https://wiki.asterisk.org/wiki/display/AST/Getting+Asterisk+Connected+to+MySQL+via+ODBC
	# https://wiki.asterisk.org/wiki/display/AST/Configuring+res_odbc
	# http://www.asteriskdocs.org/en/3rd_Edition/asterisk-book-html-chunk/installing_configuring_odbc.html#database_validating-odbc
	if grep "asterisk" /etc/odbc.ini > /dev/null ; then
		printf "%s\n" "Asterisk-related ODBC config already detected in /etc/odbc.ini"
	else
		echo "[asterisk-connector]" > /etc/odbc.ini
		echo "Description = MySQL connection to 'asterisk' database" >> /etc/odbc.ini
		echo "Driver = MariaDB" >> /etc/odbc.ini
		echo "Database = asterisk" >> /etc/odbc.ini
		echo "Server = localhost" >> /etc/odbc.ini
		echo "Port = 3306" >> /etc/odbc.ini
		echo "Socket = /var/run/mysqld/mysqld.sock" >> /etc/odbc.ini
	fi
}

paste_post() { # $1 = file to upload, $2 = 1 to delete file on success, $3 = auto-email to business office
	if [ ${#INTERLINKED_APIKEY} -eq 0 ]; then
		INTERLINKED_APIKEY=`grep -R "interlinkedkey=" $AST_CONFIG_DIR | grep -v "your-interlinked-api-key" | cut -d'=' -f2 | awk '{print $1;}' | head -n 1`
		if [ ${#INTERLINKED_APIKEY} -lt 30 ]; then
			echoerr "Failed to find InterLinked API key. For future use, please set your [globals] variables, e.g. by running phreaknet config"
			printf '\a' # alert the user
			read -r -p "InterLinked API key: " INTERLINKED_APIKEY
			if [ ${#INTERLINKED_APIKEY} -lt 30 ]; then
				echoerr "InterLinked API key too short. Not going to attempt uploading paste..."
				ls $1
				exit 2
			fi
		fi
	fi

	if [ "$3" = "1" ]; then
		url=`curl -X POST -F "body=@$1" -F "key=$INTERLINKED_APIKEY" -F "notifyuser=InterLinked" -F "expiry=259200" https://paste.interlinked.us/api/`
	else
		url=`curl -X POST -F "body=@$1" -F "key=$INTERLINKED_APIKEY" https://paste.interlinked.us/api/`
	fi

	invstring=`expr substr "${url}" 1 7` # POSIX compliant
	if [ "$invstring" = "Invalid" ]; then
		echoerr "Invalid API key"
		printf "File Location: %s\n" "$1"
	else
		printf "Paste URL: %s\n" "${url}.txt"
		if [ "$2" = "1" ]; then
			rm "$1"
		fi
	fi
}

quell_mysql() {
	if [ -f "/etc/init.d/mysql" ]; then
		if [ "$FORCE_INSTALL" = "1" ]; then
			printf "%s\n" "Restarting MySQL/MariaDB service..."
			# mysql uses up a lot of CPU and memory, and will really slow down the coredumper script (or compilation)
			service mysql restart
		fi
	fi
}

coredump_prep() {
	ensure_installed "gdb" # for astcoredumper
	# For some reason, the above is not sufficient and will lead to things like: /usr/bin/gdb does not support python
	# That's because gdb isn't really installed.
	# Use a technique aside from which/path/binary detection to see if we find something we expect:
	helplines=`gdb --help | grep "GDB manual" | wc -l`
	if [ "$helplines" != "1" ]; then
		echoerr "GDB does not appear to be currently installed."
		if [ "$PAC_MAN" = "apt-get" ]; then
			apt-get install -y gdb
		fi
	else
		printf "We think GDB is already installed... good!\n"
	fi
	# XXX If running ast_coredumper complains about a lack of python support, full gdb was not installed,
	# and the above check will succeed but we may need to run apt-get install gdb to get the full version.
	quell_mysql # we need all the CPU and memory we can get
	swap=`swapon --show | wc -l | tr -d '\n'`
	if [ "$swap" = "0" ]; then
		$FILE_PATH enable-swap # enable swap, since we'll probably run out of memory otherwise...
	fi
	freediskspace=`df / | awk '{print $4}' | tail -n +2 | tr -d '\n'`
	if [ $freediskspace -lt 700000 ]; then
		# A core dump can be around 700 MB, so ensure we have enough space to actually dump it.
		$FILE_PATH freedisk
	fi
}

get_backtrace() { # $1 = "1" to upload
	dmesg | tail -4
	coredump_prep
	printf "%s" "Core dump pattern is: "
	sysctl -n kernel.core_pattern # sysctl -w kernel.core_pattern=core
	printf "%s\n" "Finding core dump to process, this may take a moment..."
	$AST_VARLIB_DIR/scripts/ast_coredumper > /tmp/ast_coredumper.txt
	if [ $? -ne 0 ]; then
		$AST_VARLIB_DIR/scripts/ast_coredumper core > /tmp/ast_coredumper.txt
	fi
	if [ $? -ne 0 ]; then
		mostrecentcoredump=`ls -v * | grep "^core" | tail -1`
		printf "Most recent core dump: %s\n" "$mostrecentcoredump"
		$AST_VARLIB_DIR/scripts/ast_coredumper "$mostrecentcoredump" > /tmp/ast_coredumper.txt
	fi
	printf "Exit code is %d\n" $?
	corefullpath=$( grep "full.txt" /tmp/ast_coredumper.txt | cut -d' ' -f2 ) # the file is no longer necessarily simply just /tmp/core-full.txt
	if [ ! -f /tmp/ast_coredumper.txt ] || [ ${#corefullpath} -le 1 ]; then
		echoerr "No core dumps found"
		printf "%s\n" "Make sure to start asterisk with -g or set dumpcore=yes in asterisk.conf"
		exit 2
	fi
	if [ ${#corefullpath} -eq 0 ] || [ ! -f "$corefullpath" ]; then
		echoerr "Core dump failed to get backtrace, aborting..."
		printf "%s\n" "Make sure to start asterisk with -g or set dumpcore=yes in asterisk.conf"
		exit 2
	fi
	if [ "$1" = "1" ]; then # backtrace
		printf "%s\n" "Uploading paste of backtrace..."
		paste_post "$corefullpath"
		# rm /tmp/core-*.txt # actually, don't delete these, just in case...
	else # backtrace-only
		ls $corefullpath
	fi
	rm -f /tmp/ast_coredumper.txt
	ls | grep "^core" | grep -v ".txt" | xargs rm # if core dump itself wasn't automatically deleted, delete it now... but don't remove the backtraces we just extracted!
}

rule_audio() {
	cd $AST_SOUNDS_DIR # so we can use both relative and absolute paths
	# trying to use {} to combine the output of multiple commands causes issues, so do them serially
	directories="$AST_CONFIG_DIR/"
	pcregrep -rho1 --include='.*\.conf$' ',Playback\((.*)?\)' $directories | grep -vx ';.*' | cut -d',' -f1 > /tmp/phreakscript_update.txt
	pcregrep -rho1 --include='.*\.conf$' ',BackGround\((.*)?\)' $directories | grep -vx ';.*' | cut -d',' -f1 >> /tmp/phreakscript_update.txt
	pcregrep -rho1 --include='.*\.conf$' ',Read\((.*)?\)' $directories | grep -vx ';.*' | cut -s -d',' -f2 | grep -v "dial" | grep -v "stutter" | grep -v "congestion" >> /tmp/phreakscript_update.txt
	filenames=$(cat /tmp/phreakscript_update.txt | cut -d')' -f1 | tr '&' '\n' | sed '/^$/d' | sort | uniq | grep -v "\${" | xargs -d "\n" -I{} sh -c 'if ! test -f "{}.ulaw"; then echo "{}"; fi')
	lines=`echo "$filenames" | wc -l`
	if [ "$lines" -ne "0" ]; then
		echo "$filenames" > /tmp/phreakscript_results.txt
		echoerr "WARNING: Missing audio file(s) detected ($lines)"
		while read filename
		do
			grep -rnRE --include \*.conf "[,&\(]$filename" $directories | grep -v ":;" | grep -v ",dial," | grep -v ",stutter," | grep -E "Playback|BackGround|Read" | grep --color "$filename"
		done < /tmp/phreakscript_results.txt # POSIX compliant
		rm /tmp/phreakscript_results.txt
	else
		echo "No missing sound files detected..."
	fi
	rm /tmp/phreakscript_update.txt
	warnings=$(($warnings + $lines))
	cd /tmp
}

rule_invalid_jump() {
	results=$(pcregrep -ro3 --include='.*\.conf$' -hi '([1n\)]),(Goto|Gosub|GotoIf|GosubIf)\(([A-Za-z0-9_-]*),([A-Za-z0-9:\\$\{\}_-]*),([A-Za-z0-9:\\$\{\}_-]*)([\)\(:])' $AST_CONFIG_DIR/ | sort | uniq | xargs -I{} sh -c 'if ! grep -r --include \*.conf "\[{}\]" $AST_CONFIG_DIR/; then echo "Missing reference:{}"; fi' | grep "Missing reference:" | cut -d: -f2 | xargs -I{} grep -rn --include \*.conf "{}" $AST_CONFIG_DIR/ | sed 's/\s\s*/ /g' | cut -c 15- | grep -v ":;")
	lines=`echo "$results" | wc -l`
	chars=`echo "$results" | wc -c`
	if [ $chars -gt 1 ]; then # this is garbage whitespace
		if [ "$lines" -ne "0" ]; then
			echoerr "WARNING: $2 ($lines)"
			echo "$results"
		else
			echo "OK: $2"
		fi
		warnings=$(($warnings + $lines))
	fi
}

rule_unref() {
	printf "Unreachable check: Assuming all dialplan code is in subdirectories of %s...\n" $AST_CONFIG_DIR
	results=$(pcregrep -ro -hi --include='.*\.conf$' '^\[([A-Za-z0-9-])+?\]' $(ls -d $AST_CONFIG_DIR/*/) | cut -d "[" -f2 | cut -d "]" -f1 | xargs -I{} sh -c 'if ! grep -rE --include \*.conf "([ @?,:^\(=]){}"  $AST_CONFIG_DIR/; then echo "Unreachable Code:{}"; fi' | grep "Unreachable Code:" | grep -vE "\-([0-9]+)$" | cut -d: -f2 | xargs -I{} grep -rn --include \*.conf "{}" $(ls -d $AST_CONFIG_DIR/*/) | sed 's/\s\s*/ /g' | cut -c 15- | grep -v ":;")
	lines=`echo "$results" | wc -l`
	chars=`echo "$results" | wc -c`
	if [ $chars -gt 1 ]; then # this is garbage whitespace
		if [ "$lines" -ne "0" ]; then
			echoerr "WARNING: $2 ($lines)"
			echo "$results"
		else
			echo "OK: $2"
		fi
		warnings=$(($warnings + $lines))
	fi
}

rule_warning() { # $1 = rule, $2 = rule name
	results=$(grep -rnE --include \*.conf "$1" $AST_CONFIG_DIR | sed 's/\s\s*/ /g' | cut -c 15-)
	lines=`echo "$results" | wc -l`
	chars=`echo "$results" | wc -c`
	if [ $chars -gt 1 ]; then # this is garbage whitespace
		if [ "$lines" -ne "0" ]; then
			echoerr "WARNING: $2 ($lines)"
			echo "$results"
		else
			echo "OK: $2"
		fi
		warnings=$(($warnings + $lines))
	fi
}

rule_error() { # $1 = rule, $2 = rule name
	results=$(grep -rnE --include \*.conf "$1" $AST_CONFIG_DIR | sed 's/\s\s*/ /g' | cut -c 15-)
	lines=`echo "$results" | wc -l`
	chars=`echo "$results" | wc -c`
	if [ $chars -gt 1 ]; then # this is garbage whitespace
		if [ "$lines" -ne "0" ]; then
			echoerr "ERROR: $2 ($lines)"
			echo "$results"
		else
			echo "OK: $2"
		fi
		errors=$(($errors + $lines))
	fi
}

run_rules() {
	warnings=0
	errors=0
	printf "%s\n" "Processing rules..."
	# Warnings
	rule_warning '"\${(.*?)}"=""' 'Null check using expression'
	rule_warning '"\${(.*?)}"!=""' 'Existence check using expression'
	rule_warning '\$\[\${ISNULL\(([^&|]*?)\)}\]' 'Unnecessary expression surrounding ISNULL'
	rule_warning '\$\[\${EXISTS\(([^&|]*?)\)}\]' 'Unnecessary expression surrounding EXISTS'
	rule_warning '\$\[\${[A-Z]+\(([^&|=><+*/-]*?)\)}\]' 'Unnecessary expression surrounding function'
	rule_warning '\${SHELL\(curl "(.*?)"\)}' 'SHELL replacable with CURL'
	rule_warning 'ExecIf\(\$\[(.*?)\]\?Return\((.*?)\):Return\((.*?)\)\)' 'ExecIf with Return for both branches replacable with Return IF'
	rule_warning 'ExecIf\(\$\[(.*?)\]\?Return\((.*?)\)\)' 'ExecIf expression with Return for one branch replacable with ReturnIf'
	rule_warning 'ExecIf\(\$\{(.*?)\}\?Return\((.*?)\)\)' 'ExecIf function with Return for one branch replacable with ReturnIf'
	rule_warning 'ExecIf\(\$\[(.*?)\]\?Return\)' 'ExecIf function with Return replacable with ReturnIf'
	rule_warning 'ExecIf\(\$\{(.*?)\}\?Return\)' 'ExecIf function with Return replacable with ReturnIf'
	rule_warning 'Return\(\${IF\((.*?)\?1:0\)\}\)' 'IF inside RETURN is superfluous'
	rule_warning '\(\$\[([A-Z])+\(' 'Expression syntax but found function?'
	rule_warning '\w*(?<!\$);([A-Za-z ]+)?\)' 'Unmatched closing parenthesis in comment' # ignore \; because that's not a comment, it's escaped
	rule_warning ',Log\(([A-Za-z])+, ' 'Leading whitespace in log message'
	rule_warning 'Dial\(SIP/' 'SIP dial (deprecated or removed channel driver)'
	rule_invalid_jump "" "Branch to nonexistent location (missing references)"
	rule_unref "" "Context not explicitly reachable"
	rule_audio # find audio files which don't exist

	# Errors
	rule_error 'exten => ([*#\[\]a-zA-z0-9]*?),[^1]' 'Extension has no priority'
	rule_error ' n\((.*?)\),n,'  'Duplicate next for priority'
	rule_error 'same  [^n]'  'Same, but no next priority label'
	rule_error 'If\(\$\{(.*?)&(.*?)\}\?'  'Multiple functions without enclosing expression' # check for multiple functions without enclosing expression
	rule_error ' (.*?),([1n]),(.*?)\(([^\)]*) ;'  'Unterminated application call, followed by comment' # Asterisk will catch *this* error
	rule_error 'Gosub\(([-a-z0-9,]*)\(([-a-zA-Z0-9\$\{\},]*)\)([^\)])'  'Unterminated Gosub',
	rule_error 'Return\(([-a-zA-Z0-9\$\{\}\(\),]*)\}( *?);'  'Unterminated Return'
	rule_error 'exten  \['  'Extension pattern without leading underscore'
	rule_error 'exten  ([A-Za-z0-9\[]+)?(]).,'  'Extension pattern without leading underscore'
	rule_error 'exten  ([A-Za-z0-9\[]+)?(])!,'  'Extension pattern without leading underscore'
	rule_error '\w*(?<!\$)\[([A-Za-z0-9\$\{\}+-])+\]\?'  'Opening bracket without dollar sign'
	rule_error ',ExecIf\(([^?])+\?([a-z0-9])([a-z0-9]*)(:?)([a-z0-9]*)([a-z0-9]*)'  'ExecIf, but meant GotoIf?'
	rule_error ',GotoIf\(([^?])+\?([A-Z])([a-z0-9\(\)]*)(:?)([A-Z]*)([a-z0-9\(\)]*)\)'  'GotoIf, but meant ExecIf?'
	printf "%s\n" "$warnings warning(s), $errors error(s)"
}

get_source() {
	GIT_VERSION=""
	# Get latest Asterisk LTS version by default
	len=$(printf "%s" $AST_ALT_VER | wc -c)
	if [ "$AST_ALT_VER" = "master" ]; then
		AST_SOURCE_NAME="asterisk-git"
		GIT_VERSION="master"
		AST_ALT_VER="git"
		AST_MAJOR_VER=$AST_NEXT_MAJOR_VER
		printf "Proceeding to clone %s branch of Asterisk...\n" "master"
		sleep 1
	elif [ $len = 40 ]; then
		# Git commit hash
		AST_SOURCE_NAME="asterisk-git"
		GIT_VERSION=$AST_ALT_VER
		AST_ALT_VER="git"
		AST_MAJOR_VER=$AST_NEXT_MAJOR_VER # Actually, we don't know... who knows?
		printf "Proceeding to clone %s branch of Asterisk...\n" "$GIT_VERSION"
		sleep 1
	elif [ "$AST_ALT_VER" != "" ]; then
		minor_ver=`echo "$AST_ALT_VER" | cut -c 3-`
		if [ "$minor_ver" = "" ]; then
			# Only a major version specified, use the latest version available for that major version (assume it's supported)
			AST_SOURCE_NAME="asterisk-$AST_ALT_VER-current"
			AST_MAJOR_VER=$AST_ALT_VER
		else
			# Install a specific major.minor.patch version
			AST_SOURCE_NAME="asterisk-$AST_ALT_VER"
			AST_MAJOR_VER=`echo "$AST_ALT_VER" | cut -c 1-2`
		fi
		printf "%s\n" "Proceeding to install Asterisk $AST_ALT_VER (~major $AST_MAJOR_VER)..."
		if [ $AST_MAJOR_VER -lt $AST_MIN_PREFERRED_VER ]; then
			echoerr "***************************** WARNING *****************************"
			printf "%s\n" "PhreakScript IS NOT TESTED WITH OLDER VERSIONS OF ASTERISK!!!"
			printf "%s\n" "Proceed at your own risk..."
			sleep 1
		fi
	fi
	EFF_SOURCE_NAME=$AST_SOURCE_NAME
	# Remove any previous version to ensure wget doesn't add a suffix to the filename
	rm -f $EFF_SOURCE_NAME.tar.gz # the name itself doesn't guarantee that the version is the same
	if [ "$AST_ALT_VER" = "" ]; then # download latest bundled version
		if [ "$PREFER_RELEASE_CANDIDATES" = "1" ]; then
			# Delete any previous release candidates
			rm -f $AST_RC_SOURCE_NAME.tar.gz # the name itself doesn't guarantee that the version is the same
			$WGET https://downloads.asterisk.org/pub/telephony/asterisk/$AST_RC_SOURCE_NAME.tar.gz
			if [ ! -f $AST_RC_SOURCE_NAME.tar.gz ]; then
				printf "No release candidate is currently available, installing latest stable version instead\n"
				PREFER_RELEASE_CANDIDATES=0
			else
				EFF_SOURCE_NAME=$AST_RC_SOURCE_NAME
			fi
		fi
		if [ "$PREFER_RELEASE_CANDIDATES" = "0" ]; then
			$WGET https://downloads.asterisk.org/pub/telephony/asterisk/$AST_SOURCE_NAME.tar.gz
		fi
	elif [ "$AST_ALT_VER" = "git" ]; then # clone master branch
		if [ -d "asterisk" ]; then
			if [ "$FORCE_INSTALL" = "1" ]; then
				rm -rf "asterisk"
			else
				die "Directory asterisk already exists. Please rename or delete this directory and restart installation or specify the force flag."
			fi
		fi
		if [ -d "asterisk-git" ]; then
			cd asterisk-git
			git reset --hard origin/master
			cd ..
		else
			git clone --depth=1 "https://github.com/asterisk/asterisk"
			mv asterisk asterisk-git
		fi
		if [ $? -ne 0 ]; then
			die "Failed to clone asterisk repository"
		fi
	else
		$WGET https://downloads.asterisk.org/pub/telephony/asterisk/releases/$AST_SOURCE_NAME.tar.gz
		# If someone is downloading the current version, it might not be in the releases directory, which is for archived (old) releases only
		if [ $? -ne 0 ]; then
			$WGET https://downloads.asterisk.org/pub/telephony/asterisk/$AST_SOURCE_NAME.tar.gz
		fi
	fi
	if [ $? -ne 0 ]; then
		if [ "$AST_ALT_VER" != "git" ]; then
			echoerr "Failed to download file: https://downloads.asterisk.org/pub/telephony/asterisk/releases/$AST_SOURCE_NAME.tar.gz"
		fi
		if [ "$AST_ALT_VER" != "" ]; then
			printf "It seems Asterisk version %s does not exist...\n" "$AST_ALT_VER"
		fi
		exit 1
	fi
	if [ "$CHAN_SCCP" = "1" ]; then
		git clone --depth=1 https://github.com/chan-sccp/chan-sccp.git chan-sccp
	fi

	if [ "$AST_ALT_VER" = "git" ]; then
		AST_SRC_DIR="asterisk-git"
		AST_MM_VER=999999
	else
		AST_SRC_DIR=`tar -tzf $EFF_SOURCE_NAME.tar.gz | head -1 | cut -f1 -d"/"`
		if [ -d "$AST_SOURCE_PARENT_DIR/$AST_SRC_DIR" ]; then
			if [ "$FORCE_INSTALL" = "1" ]; then
				rm -rf $AST_SRC_DIR
			else
				die "Directory $AST_SRC_DIR already exists. Please rename or delete this directory and restart installation or specify the force flag."
			fi
		fi
		ver=`echo $AST_SRC_DIR | cut -d'-' -f2`
		major=`echo $ver | cut -d'.' -f1`
		minor=`echo $ver | cut -d'.' -f2`
		patch=`echo $ver | cut -d'.' -f3`
		AST_MM_VER=`printf "%02d%02d%02d" "$major" "$minor" "$patch"`
		printf "Asterisk MMmmPP: %s\n" "$AST_MM_VER"
	fi
	if [ "$AST_SRC_DIR" = "" ]; then
		die "No Asterisk source directory available, aborting..."
	fi
	printf "%s\n" "Installing version $AST_SRC_DIR..."
	if [ "$AST_ALT_VER" != "git" ]; then
		tar -zxvf $EFF_SOURCE_NAME.tar.gz
		rm $EFF_SOURCE_NAME.tar.gz
	fi
	if [ ! -d $AST_SOURCE_PARENT_DIR/$AST_SRC_DIR ]; then
		printf "Directory not found: %s\n" "$AST_SOURCE_PARENT_DIR/$AST_SRC_DIR"
		exit 2
	fi
	AST_SRC_DIR2=`get_newest_astdir`
	AST_SRC_DIR2=`printf "%s" "$AST_SRC_DIR2" | cut -d'/' -f1`
	while [ "$AST_SRC_DIR" != "$AST_SRC_DIR2" ]; do
		if [ "$FORCE_INSTALL" = "1" ]; then
			echoerr "Deleting $AST_SRC_DIR2 to prevent ordering conflict"
			rm -rf $AST_SRC_DIR2
		else
			echoerr "Directory $AST_SRC_DIR2 conflicts with installation of $AST_SRC_DIR. Please rename or delete and restart installation."
			exit 2
		fi
		sleep 1 # don't tight loop in case something goes wrong.
		AST_SRC_DIR2=`get_newest_astdir`
		AST_SRC_DIR2=`printf "%s" "$AST_SRC_DIR2" | cut -d'/' -f1`
	done
	cd $AST_SOURCE_PARENT_DIR/$AST_SRC_DIR
	if [ "$GIT_VERSION" != "" ]; then
		git checkout $GIT_VERSION
		$AST_MAKE clean # git reset and git checkout aren't sufficient, we need to 'make clean' as well
	fi
	if [ ! -f channels/chan_sip.c ]; then
		printf "chan_sip was not natively present in this version of Asterisk\n"
		ENHANCED_CHAN_SIP=1 # chan_sip isn't present anymore, we need to readd it ourselves (if we're going to build chan_sip at all)
	fi
	if [ "$CHAN_SIP" = "1" ]; then # somebody still wants chan_sip, okay...
		if [ "$ENHANCED_CHAN_SIP" != "1" ]; then
			echoerr "chan_sip is deprecated and was removed in Asterisk 21. Consider migrating to chan_pjsip at your convenience."
		else
			echoerr "chan_sip was deprecated and removed in Asterisk 21. It is still present for your usage, but consider migrating to chan_pjsip at your convenience."
			printf "Fetching chan_sip to readd to source tree\n"
			wget https://raw.githubusercontent.com/InterLinked1/chan_sip/master/chan_sip_reinclude.sh
			chmod +x chan_sip_reinclude.sh
			./chan_sip_reinclude.sh
		fi
	fi
	if [ "$ALSA" = "1" ]; then
		# chan_alsa was removed in Asterisk 21, and with it, the support for ALSA lib detection in the build system. Add it back if needed.
		lines=$(grep "HAVE_ALSA" include/asterisk/autoconfig.h | wc -l)
		if [ $lines -eq 0 ]; then
			printf "Patching build system to detect ALSA library\n"
			git_patch "alsa.diff"
			./bootstrap.sh # Regenerate configure and include/asterisk/autoconfig.h.in
		fi
	fi
	if [ "$SIP_CISCO" = "1" ]; then # ASTERISK-13145 (https://issues.asterisk.org/jira/browse/ASTERISK-13145)
		if [ ! -f channels/chan_sip.c ]; then
			die "chan_sip is not present in this source, please add the --sip option"
		fi
		# https://usecallmanager.nz/patching-asterisk.html and https://github.com/usecallmanagernz/patches
		wget -q "https://raw.githubusercontent.com/usecallmanagernz/patches/master/asterisk/$CISCO_CM_SIP.patch" -O /tmp/$CISCO_CM_SIP.patch
		patch --strip=1 < /tmp/$CISCO_CM_SIP.patch
		if [ $? -ne 0 ]; then
			echoerr "WARNING: Call Manager patch may have failed to apply correctly"
		fi
		rm /tmp/$CISCO_CM_SIP.patch
		CFLAGS="-DENABLE_SRTP_AES_GCM -DENABLE_SRTP_AES_256" ./configure
	fi
	# XXX run this manually and trust the cert since the SVN server seems to have cert validation issues occasionally
	# Do this BEFORE running the script so that the script will see the files present and just patch and exit.
	svn --non-interactive --trust-server-cert export https://svn.digium.com/svn/thirdparty/mp3/trunk addons/mp3
	./contrib/scripts/get_mp3_source.sh

	if [ "$EXTRA_FEATURES" = "1" ]; then
		# Add PhreakNet patches
		printf "%s\n" "Beginning custom patches..."
		phreak_patches $PATCH_DIR $AST_SRC_DIR # custom patches
		printf "%s\n" "Custom patching completed..."
	else
		echoerr "Skipping feature patches..."
	fi
	if [ "$OS_DIST_INFO" = "FreeBSD" ]; then
		freebsd_port_patches
	fi
}

# Minimum argument check
if [ $# -lt $MIN_ARGS ]; then
	cmd="help"
else
	cmd="$1"
fi

FLAG_TEST=0
PARSED_ARGUMENTS=$(getopt -n phreaknet -o bc:u:dfhostu:v:w -l backtraces,cc:,dahdi,force,flag-test,help,sip,testsuite,user:,version:,weaktls,alsa,cisco,sccp,clli:,debug:,devmode,disa:,drivers,experimental,extcodecs,fast,freepbx,generic,autokvers,lightweight,api-key:,rotate,audit,boilerplate,upstream:,manselect,minimal,vanilla,wanpipe -- "$@")
VALID_ARGUMENTS=$?
if [ "$VALID_ARGUMENTS" != "0" ]; then
	usage
	exit 2
fi

if [ "$OS_DIST_INFO" != "FreeBSD" ]; then
	eval set -- "$PARSED_ARGUMENTS"
else
	shift # for FreeBSD
fi

while true; do
	if [ "$1" = "" ]; then
		break # for FreeBSD
	fi
	case "$1" in
		-b | --backtraces ) ENABLE_BACKTRACES=1; shift ;;
        -c | --cc ) AST_CC=$2; shift 2;;
		-d | --dahdi ) CHAN_DAHDI=1; shift ;;
		-f | --force ) FORCE_INSTALL=1; IGNORE_FAILURES=1; shift ;;
		-h | --help ) cmd="help"; shift ;;
		-n | --no-rc ) PREFER_RELEASE_CANDIDATES=0; shift ;;
		-o | --flag-test ) FLAG_TEST=1; shift;;
		-s | --sip ) CHAN_SIP=1; shift ;;
		-t | --testsuite ) TEST_SUITE=1; DEVMODE=1; shift ;;
		-u | --user ) AST_USER=$2; shift 2;;
		-v | --version ) AST_ALT_VER=$2; shift 2;;
		-w | --weaktls ) WEAK_TLS=1; shift ;;
		--alsa ) ALSA=1; shift ;;
		--audit ) PKG_AUDIT=1; shift ;;
		--cisco ) SIP_CISCO=1; shift ;;
		--sccp ) CHAN_SCCP=1; shift ;;
		--boilerplate ) BOILERPLATE_SOUNDS=1; shift ;;
		--clli ) PHREAKNET_CLLI=$2; shift 2;;
		--disa ) PHREAKNET_DISA=$2; shift 2;;
		--debug ) DEBUG_LEVEL=$2; shift 2;;
		--devmode ) DEVMODE=1; shift ;;
		--drivers ) DAHDI_OLD_DRIVERS=1; shift ;;
		--experimental ) EXPERIMENTAL_FEATURES=1; shift ;;
		--extcodecs ) EXTERNAL_CODECS=1; shift ;;
		--fast ) FAST_COMPILE=1; shift ;;
		--freepbx ) FREEPBX_GUI=1; shift ;;
		--generic ) GENERIC_HEADERS=1; shift ;;
		--autokvers ) AUTOSET_KVERS=1; shift ;;
		--lightweight ) LIGHTWEIGHT=1; shift ;;
		--api-key ) INTERLINKED_APIKEY=$2; shift 2;;
		--rotate ) ASTKEYGEN=1; shift ;;
		--upstream ) SCRIPT_UPSTREAM=$2; shift 2;;
		--manselect ) MANUAL_MENUSELECT=1; shift ;;
		--minimal ) ENHANCED_INSTALL=0; shift ;;
		--vanilla ) EXTRA_FEATURES=0; shift ;;
		--wanpipe ) DAHDI_WANPIPE=1; shift ;;
		# -- means the end of the arguments; drop this, and break out of the while loop
		--) shift; break ;;
		# If invalid options were passed, then getopt should have reported an error,
		# which we checked as VALID_ARGUMENTS when getopt was called...
		*) die "Unexpected option: $1"
		   #cmd="help"; shift; break ;;
	esac
done

if [ "$FAST_COMPILE" = "1" ]; then
	AST_MAKE="nice -15 $AST_MAKE" # -15 to speed up compilation by increasing CPU priority.
fi

if [ "$FLAG_TEST" = "1" ]; then
	echog "Flag test successful."
	exit 0
fi

if [ -z "${AST_CC##*[!0-9]*}" ]; then # POSIX compliant: https://unix.stackexchange.com/a/574169/
	echoerr "Country code must be an integer."
	exit 1
fi

if which curl > /dev/null; then # only execute if we have curl
	self=`grep "# v" $FILE_PATH | head -1 | cut -d'v' -f2`
	# Only download the first few lines of the file, to get the latest version number, and only check every so often
	recent=$( find /tmp/phreaknet_upstream_version.txt -mmin -720 2>/dev/null | wc -l )
	if [ "$recent" != "1" ]; then
		printf " *** Checking if a higher numbered version is available..."
		curl --silent https://raw.githubusercontent.com/InterLinked1/phreakscript/master/phreaknet.sh -r 0-210 > /tmp/phreaknet_upstream_version.txt
		# Compare current version with latest upstream version
		upstream=`grep -m1 "# v" /tmp/phreaknet_upstream_version.txt | cut -d'v' -f2`
		if [ "$self" != "$upstream" ] && [ "$cmd" != "update" ]; then
			# Pull from GitHub here for version check for a) speed and b) stability reasons
			# In this case, it's possible that our version number could actually be ahead of master, since we update from the dev upstream. We need to compare the versions, not merely check if they differ.
			# Don't throw a warning if only the development tip is ahead of the master branch. But if master is ahead of us, then warn.
			morerecent=`printf "%s\n%s" "$self" "$upstream" | sort | tail -1`
			if [ "${morerecent}" != "${self}" ]; then
				printf " YES\n"
				echoerr "WARNING: PhreakScript is out of date (most recent stable version is $upstream) - run 'phreaknet update' to update"
				sleep 0.5
			else
				printf " NO (AHEAD)\n"
			fi
		else
			printf " NO\n"
		fi
	fi
fi

dialog_result() {
	if [ "$1" = "$2" ]; then
		wizardresult="$wizardresult $3"
	fi
}

require_installed_asterisk() {
	if [ ! -d $AST_VARLIB_DIR ]; then
		die "$AST_VARLIB_DIR does not exist; please install Asterisk before running this commands"
	fi
	if [ ! -d $AST_SOUNDS_DIR ]; then
		die "$AST_SOUNDS_DIR does not exist; please install Asterisk before running this commands"
	fi
}

if [ "$cmd" = "help" ]; then
	usage
	exit 2
elif [ "$cmd" = "wizard" ]; then
	wizardresult=""
	ensure_installed "dialog"

	# DAHDI
	ans=$(dialog --nocancel --menu "Do you need support for traditional telephony equipment? (DAHDI)" 20 60 12 y Yes n No 2>&1 >/dev/tty)
	dialog_result "$ans" "y" "--dahdi"
	if [ "$ans" = "y" ]; then
		ans=$(dialog --nocancel --default-item 'n' --menu "Do you need drivers for any of the following cards?\nWCTDM800, WCAEX800, WCTDM410, WXAEX410, WXTE120XP, WCTE11XP, WCTDM, WCT1XXP, WCFXO" 20 60 12 y Yes n No 2>&1 >/dev/tty)
	fi
	dialog_result "$ans" "y" "--drivers"

	# Channel Drivers
	ans=$(dialog --nocancel --default-item 'n' --menu "Do you require ALSA library support?" 20 60 12 y Yes n No 2>&1 >/dev/tty)
	dialog_result "$ans" "y" "--alsa"
	ans=$(dialog --nocancel --default-item 'n' --menu "Do you need support for the deprecated chan_sip (SIP) module?" 20 60 12 y Yes n No 2>&1 >/dev/tty)
	dialog_result "$ans" "y" "--sip"
	ans=$(dialog --nocancel --default-item 'n' --menu "Do you want to install the chan_sccp (Skinny) channel driver?" 20 60 12 y Yes n No 2>&1 >/dev/tty)
	dialog_result "$ans" "y" "--sccp"
	ans=$(dialog --nocancel --default-item 'n' --menu "Do you want to install the usecallmanager chan_sip patches?" 20 60 12 y Yes n No 2>&1 >/dev/tty)
	dialog_result "$ans" "y" "--cisco"

	# General
	ans=$(dialog --nocancel --default-item 'n' --menu "Do you need support for TLS 1.0?" 20 60 12 y Yes n No 2>&1 >/dev/tty)
	dialog_result "$ans" "y" "--weaktls"
	ans=$(dialog --nocancel --menu "Do you wish to overwrite/upgrade an existing Asterisk install?" 20 60 12 y Yes n No 2>&1 >/dev/tty)
	dialog_result "$ans" "y" "--force"
	ans=$(dialog --nocancel --default-item 'n' --menu "Do you want to install the FreePBX GUI? (not recommended)" 20 60 12 y Yes n No 2>&1 >/dev/tty)
	dialog_result "$ans" "y" "--freepbx"

	# "Advanced" options
	ans=$(dialog --nocancel --menu "Do you want to to configure additional advanced options?\n" 20 60 12 y Yes n No 2>&1 >/dev/tty)
	if [ "$ans" = "y" ]; then
		ans=$(dialog --nocancel --default-item 'n' --menu "Do you want to install Asterisk to run as another user?" 20 60 12 y Yes n No 2>&1 >/dev/tty)
		if [ "$ans" = "y" ]; then
			ans=$(dialog --nocancel --inputbox "What user do you want to run Asterisk under?" 20 60 "asterisk" 2>&1 >/dev/tty)
			wizardresult="$wizardresult --user=$ans"
		fi
		ans=$(dialog --nocancel --inputbox "What version of Asterisk do you want to install?\n e.g. latestlts = latest LTS version, master = Git master, 18.12.0 = 18.2.0" 20 60 "latestlts" 2>&1 >/dev/tty)
		if [ "$ans" != "latestlts" ]; then
			wizardresult="$wizardresult --version=$ans"
		fi
		ans=$(dialog --nocancel --inputbox "Your country code?" 20 60 "1" 2>&1 >/dev/tty)
		if [ "$ans" != "1" ]; then
			wizardresult="$wizardresult --cc=$ans"
		fi
		ans=$(dialog --nocancel --default-item 'n' --menu "Do you want to enable backtraces for debugging?" 20 60 12 y Yes n No 2>&1 >/dev/tty)
		dialog_result "$ans" "y" "--backtraces"
		ans=$(dialog --nocancel --default-item 'n' --menu "Do you want to install in development mode?\nThis will install the test framework and test suite, and compile with certain development compilation options." 20 60 12 y Yes n No 2>&1 >/dev/tty)
		dialog_result "$ans" "y" "--testsuite"
		ans=$(dialog --nocancel --default-item 'n' --menu "Do you want to manually run menuselect?" 20 60 12 y Yes n No 2>&1 >/dev/tty)
		dialog_result "$ans" "y" "--manselect"
		ans=$(dialog --nocancel --default-item 'n' --menu "Do you or will you require support for external codecs?" 20 60 12 y Yes n No 2>&1 >/dev/tty)
		dialog_result "$ans" "y" "--extcodecs"
		ans=$(dialog --nocancel --default-item 'n' --menu "Do you want to install only generic Asterisk, without any PhreakNet enhancements?" 20 60 12 y Yes n No 2>&1 >/dev/tty)
		dialog_result "$ans" "y" "--vanilla"
		ans=$(dialog --nocancel --default-item 'n' --menu "Do you want to install only basic modules (may significantly degrade functionality)?" 20 60 12 y Yes n No 2>&1 >/dev/tty)
		dialog_result "$ans" "n" "--lightweight"
		ans=$(dialog --nocancel --default-item 'n' --menu "Do you want to install experimental features that may not be production ready?" 20 60 12 y Yes n No 2>&1 >/dev/tty)
		dialog_result "$ans" "n" "--experimental"
		ans=$(dialog --nocancel --default-item 'n' --menu "Do you want to prevent installation of nonrequired dependencies?" 20 60 12 y Yes n No 2>&1 >/dev/tty)
		dialog_result "$ans" "y" "--minimal"
		ans=$(dialog --nocancel --default-item 'n' --menu "Do you want to audit package installation?" 20 60 12 y Yes n No 2>&1 >/dev/tty)
		dialog_result "$ans" "y" "--audit"
	fi

	ans=$(dialog --nocancel --default-item 'n' --menu "Last question! Do you want to begin installation automatically?" 20 60 12 y Yes n No 2>&1 >/dev/tty)

	printf "\n\n"
	echog "You have completed the installation command wizard. You may use the following command to install according to your preferences:"
	printf "phreaknet install %s\n" "$wizardresult"
	if [ "$ans" = "y" ]; then
		if which phreaknet > /dev/null; then
			printf "Automatically installing with the determined options...\n"
			sleep 1
			exec phreaknet install "$wizardresult"
		else
			echoerr "Could not find PhreakScript in PATH. Please ensure this is run as root, and manually run the above command."
		fi
	fi
elif [ "$cmd" = "make" ]; then
	# chmod +x phreaknet.sh
	# ./phreaknet.sh make
	# phreaknet install

	assert_root
	if [ "$FILE_PATH" = "/usr/local/sbin/phreaknet" ]; then
		die "PhreakScript is already installed, to reinstall, execute 'make' using the new script, e.g. ./phreaknet.sh make"
	fi
	ln -nsf $FILE_PATH /usr/local/sbin/phreaknet
	if [ $? -eq 0 ]; then
		echo "PhreakScript added to path."
	else
		echo "PhreakScript could not be added to path. Is it already there?"
		echo "If it's not, move the source file (phreaknet.sh) to /usr/local/src and try again"
	fi
elif [ "$cmd" = "man" ]; then
	cd /tmp
	wget "https://raw.githubusercontent.com/InterLinked1/phreakscript/master/phreaknet.1.md"
	ensure_installed pandoc
	# f option required for double dash to work correctly: https://github.com/jgm/pandoc/issues/5404
	pandoc phreaknet.1.md -f markdown-smart -s -t man -o phreaknet.1
	if [ ! -d /usr/local/man/man1 ]; then
		mkdir /usr/local/man/man1
	fi
	cp phreaknet.1 /usr/local/man/man1
	gzip /usr/local/man/man1/phreaknet.1
	mandb
elif [ "$cmd" = "mancached" ]; then
	cd /tmp
	wget "https://raw.githubusercontent.com/InterLinked1/phreakscript/master/phreaknet.1"
	if [ ! -d /usr/local/man/man1 ]; then
		mkdir /usr/local/man/man1
	fi
	cp phreaknet.1 /usr/local/man/man1
	gzip /usr/local/man/man1/phreaknet.1
	mandb
elif [ "$cmd" = "source" ]; then
	if [ $(id -u) -ne 0 ]; then
		# check for source prereqs: git and svn
		{ git --version >/dev/null && svn help >/dev/null; } || { echoerr "Please install git and subversion packages and try again."; exit 2; }
	else
		# we are already root, so just install prereqs
		install_prereq 1
	fi
	AST_SOURCE_PARENT_DIR=$PWD
	get_source
elif [ "$cmd" = "experimental" ]; then
	assert_root
	cd $AST_SOURCE_PARENT_DIR
	AST_SRC_DIR=`get_newest_astdir`
	if [ $AST_MAJOR_VER -ge 21 ]; then
		printf "Installing 21+ patches for experimental features\n"
		add_experimental
		$AST_MAKE
	else
		echoerr "Your version of Asterisk is not eligible for experimental features"
	fi
elif [ "$cmd" = "install" ]; then
	preinstall_warn=0

	if [ "$RTPULSING" != "1" ]; then
		echoerr "Real time pusling is not compatible or has been disabled for this build."
		sleep 1
	fi
	if [ "$HAVE_COMPATIBLE_SPANDSP" != "1" ]; then
		echoerr "SpanDSP is not compatible or has been disabled for this build."
		sleep 1
	fi

	if [ ${#AST_USER} -eq 0 ]; then
		echoerr "WARNING: You are installing Asterisk to run as root. This is not recommended."
		echoerr "Specify -u or --user to specify a run user"
		printf "\n"
		preinstall_warn=1
	fi
	if [ "$LIGHTWEIGHT" = "1" ]; then
		echoerr "WARNING: You have specified --lightweight, which will disable most modules from building."
		echoerr "This may result in a system that is not suitable for production usage."
		printf "\n"
		preinstall_warn=1
	fi
	if [ "$EXTERNAL_CODECS" = "1" ] && [ "$AST_ALT_VER" = "git" ]; then
		echoerr "WARNING: External codecs should not be installed with the master branch."
		echoerr "This may cause ABI (Application Binary Interface) issues, including crashes."
		printf "\n"
		preinstall_warn=1
	fi
	if [ "$preinstall_warn" = "1" ]; then
		sleep 2
	fi
	if [ "$PKG_AUDIT" = "1" ]; then
		if [ "$PAC_MAN" = "apt-get" ]; then
			pkg_before=$( apt list --installed )
		else
			echoerr "Distro not supported for package audit"
		fi
	fi

	# Install Pre-Reqs
	printf "%s %d\n" "Starting installation with country code" $AST_CC
	quell_mysql
	install_prereq 1 # This must be done before any other packages are installed since we'll skip package install checks if package manager was used recently.
	if [ "$DEVMODE" = "1" ]; then
		# Install the Linux headers if we can, but don't abort if we can't.
		if [ "$PAC_MAN" = "apt-get" ]; then
			apt-get install -y linux-headers-`uname -r`
			if [ $? -ne 0 ]; then # we're not installing DAHDI, but warn about this so we know we can't.
				echoerr "DAHDI is potentially incompatible with this system (missing kernel build headers)"
				linux_headers_info
			fi
		fi
	fi
	assert_root
	if [ "$FREEPBX_GUI" = "1" ]; then
		install_freepbx_checks
	fi
	freediskspace=`df / | awk '{print $4}' | tail -n +2 | tr -d '\n'` # free disk space, in KB
	if [ ${#freediskspace} -gt 0 ]; then
		if [ $freediskspace -lt 800000 ]; then # warn users if they're about to install Asterisk with insufficient disk space ( < 800,000 KB)
			echoerr "WARNING: Free disk space is low: "
			df --local --type=ext4 -h
			if [ "$FORCE_INSTALL" = "1" ]; then
				sleep 3
			else
				exit 1
			fi
		fi
	fi
	cd $AST_SOURCE_PARENT_DIR
	# Get DAHDI
	if [ "$CHAN_DAHDI" = "1" ]; then
		if [ ! -d /etc/dahdi ] || [ "$FORCE_INSTALL" = "1" ]; then
			install_dahdi
		else
			echoerr "DAHDI already present but install not forced, skipping..."
			sleep 2
		fi
	fi
	cd $AST_SOURCE_PARENT_DIR
	get_source
	# Install Pre-Reqs
	if [ "$PAC_MAN" = "apt-get" ]; then
		printf "%s %d" "libvpb1 libvpb1/countrycode string" "$AST_CC" | debconf-set-selections -v
		apt-get install -y libvpb1
	fi
	./contrib/scripts/install_prereq install

	if [ "$DEVMODE" = "1" ]; then
		configure_devmode
	else
		./configure --with-jansson-bundled --with-pjproject-bundled
	fi
	if [ $? -ne 0 ]; then
		exit 2
	fi
	cp contrib/scripts/voicemailpwcheck.py /usr/local/bin
	chmod +x /usr/local/bin/voicemailpwcheck.py

	# Change Compile Options: https://wiki.asterisk.org/wiki/display/AST/Using+Menuselect+to+Select+Asterisk+Options
	$AST_MAKE menuselect.makeopts
	menuselect/menuselect --enable format_mp3 menuselect.makeopts # add mp3 support
	# Apply menuselects for patches
	if [ "$EXTRA_FEATURES" = "1" ]; then
		menuselect/menuselect --enable app_memory --enable res_cliexec menuselect.makeopts # enable modules that are not built by default
	fi
	# We want ulaw, not gsm (default)
	menuselect/menuselect --disable-category MENUSELECT_MOH --disable-category MENUSELECT_CORE_SOUNDS --disable-category MENUSELECT_EXTRA_SOUNDS menuselect.makeopts
	# Only grab sounds if this is the first time
	if [ ! -d "$AST_SOUNDS_DIR" ]; then
		menuselect/menuselect --enable CORE-SOUNDS-EN-ULAW --enable MOH-OPSOUND-ULAW --enable EXTRA-SOUNDS-EN-ULAW menuselect.makeopts # get the ulaw audio files...
	else
		echo "Sounds already installed, skipping installation of sound files."
	fi
	if [ "$ENABLE_BACKTRACES" = "1" ]; then
		menuselect/menuselect --enable DONT_OPTIMIZE --enable BETTER_BACKTRACES menuselect.makeopts
	fi
	# If $CHAN_DAHDI is 1, then /etc/dahdi should already exist. This will ensure these are enabled if DAHDI already was present and we're not upgrading it now.
	if [ -d /etc/dahdi ]; then
		# in reality, this will never fail, even if they can't be enabled...
		menuselect/menuselect --enable chan_dahdi --enable app_meetme --enable app_flash menuselect.makeopts
	fi
	if [ "$CHAN_SIP" = "1" ]; then # somebody still wants chan_sip, okay...
		menuselect/menuselect --enable chan_sip menuselect.makeopts
	else
		if [ $AST_MAJOR_VER -lt 21 ]; then
			menuselect/menuselect --disable chan_sip menuselect.makeopts
		fi
	fi
	# in 19+, ADSI is not built by default. We should always build and enable it.
	menuselect/menuselect --enable res_adsi --enable app_adsiprog --enable app_getcpeid menuselect.makeopts
	if [ $AST_MAJOR_VER -lt 21 ]; then
		# Disable the built-in skinny and mgcp modules, since there are better alternatives, and they're deprecated as of 19
		menuselect/menuselect --disable chan_skinny --disable chan_mgcp menuselect.makeopts
		# Who's actually using this?
		menuselect/menuselect --disable app_osplookup menuselect.makeopts
	fi

	# Expand TLS support from 1.2 to 1.0 for older ATAs, if needed
	if [ "$WEAK_TLS" = "1" ]; then
		sed -i 's/TLSv1.2/TLSv1.0/g' /etc/ssl/openssl.cnf
		sed -i 's/DEFAULT@SECLEVEL=2/DEFAULT@SECLEVEL=1/g' /etc/ssl/openssl.cnf
		printf "%s\n" "Successfully patched OpenSSL to allow TLS 1.0..."
	fi

	if [ "$LIGHTWEIGHT" = "1" ]; then
		printf "Disabling most modules from building...\n"
		# This is less intended for lean production systems than for situations where we may want to recompile Asterisk a very large number of times,
		# and we can afford to exclude modules to compile as fast as possible.

		# Start by disabling almost everything.
		menuselect/menuselect --disable-category MENUSELECT_ADDONS --disable-category MENUSELECT_APPS menuselect.makeopts
		menuselect/menuselect --disable-category MENUSELECT_CDR --disable-category MENUSELECT_CEL menuselect.makeopts
		menuselect/menuselect --disable-category MENUSELECT_CHANNELS --disable-category MENUSELECT_CODECS menuselect.makeopts
		menuselect/menuselect --disable-category MENUSELECT_FORMATS --disable-category MENUSELECT_FUNCS --disable-category MENUSELECT_PBX --disable-category MENUSELECT_RES menuselect.makeopts
		menuselect/menuselect --disable-category MENUSELECT_TESTS --disable-category MENUSELECT_CFLAGS menuselect.makeopts
		menuselect/menuselect --disable-category MENUSELECT_UTILS --disable-category MENUSELECT_AGIS menuselect.makeopts

		# Now explicitly enable things we probably want.
		# Core
		menuselect/menuselect --enable app_bridgeaddchan --enable app_channelredirect --enable app_chanspy --enable app_confbridge --enable app_dial --enable app_exec menuselect.makeopts
		menuselect/menuselect --enable app_flash --enable app_mixmonitor --enable app_originate --enable app_playback --enable app_playtones --enable app_read menuselect.makeopts
		menuselect/menuselect --enable chan_bridge_media --enable chan_dahdi --enable chan_iax2 --enable chan_pjsip menuselect.makeopts
		menuselect/menuselect --enable codec_a_mu --enable codec_dahdi --enable codec_ulaw menuselect.makeopts
		menuselect/menuselect --enable format_pcm --enable format_sln --enable format_wav menuselect.makeopts
		menuselect/menuselect --enable func_callerid --enable func_cdr --enable func_channel --enable func_curl --enable func_cut --enable func_db --enable func_global menuselect.makeopts
		menuselect/menuselect --enable func_pjsip_aor --enable func_pjsip_contact --enable func_pjsip_endpoint --enable func_shell --enable func_volume menuselect.makeopts
		menuselect/menuselect --enable pbx_config --enable pbx_spool menuselect.makeopts
		menuselect/menuselect --enable res_clialiases --enable res_clioriginate --enable res_crypto --enable res_curl --enable res_fax menuselect.makeopts
		menuselect/menuselect --enable res_pjsip --enable res_pjsip_acl --enable res_pjsip_authenticator_digest --enable res_pjsip_caller_id menuselect.makeopts
		menuselect/menuselect --enable res_pjsip_dtmf_info --enable res_pjsip_endpoint_identifier_ip --enable res_pjsip_endpoint_identifier_user menuselect.makeopts
		menuselect/menuselect --enable res_pjsip_header_funcs --enable res_pjsip_logger --enable res_pjsip_notify --enable res_pjsip_outbound_registration menuselect.makeopts
		menuselect/menuselect --enable res_pjsip_pubsub --enable res_pjsip_session --enable res_rtp_asterisk menuselect.makeopts
		menuselect/menuselect --enable res_sorcery_astdb --enable res_sorcery_config --enable res_sorcery_memory --enable res_sorcery_memory_cache menuselect.makeopts
		menuselect/menuselect --enable res_srtp --enable res_timing_timerfd menuselect.makeopts
		menuselect/menuselect --enable res_geolocation --enable res_statsd menuselect.makeopts # required for res_pjsip to load
		# Extended
		menuselect/menuselect --enable app_stack --enable app_if menuselect.makeopts
		menuselect/menuselect --enable func_frame_trace menuselect.makeopts
		menuselect/menuselect --enable res_cliexec menuselect.makeopts
		# "Deprecated"
		menuselect/menuselect --enable app_adsiprog --enable app_getcpeid menuselect.makeopts
		menuselect/menuselect --enable res_adsi menuselect.makeopts
	else
		# Disable non pbx_config config modules to avoid dialplan conflict issues.
		menuselect/menuselect --disable pbx_ael --disable pbx_lua menuselect.makeopts
	fi

	if [ "$MANUAL_MENUSELECT" = "1" ]; then
		$AST_MAKE menuselect # allow user to run menuselect manually if requested. This will block until user finishes.
	fi

	# Compile Asterisk
	if [ "$AST_MAKE" = "gmake" ]; then # jansson fails to compile nicely on its own with gmake
		cd third-party/jansson
		gmake
		cd ../..
	fi

	if [ "$OS_DIST_INFO" = "FreeBSD" ]; then
		# For some reason, %%LIBSYSINFO%% is in the linking flags on FreeBSD, remove that from being added. libsysinfo is needed though.
		# Same with HAVE_CRYPT_R, that's not available but gets detected, so undetect it
		# Note that these sed expressions are designed for BSD sed, and do not work with GNU sed
		sed -i '' '/LIBSYSINFO/d' main/Makefile
		sed -i '' '/HAVE_CRYPT_R/d' include/asterisk/autoconfig.h
		sed -i "" -e 's|WRAP_LIBC_MALLOC|ASTMM_LIBC ASTMM_REDIRECT|g' addons/mp3/interface.c # for format_mp3
		sed -i "" -e 's|\\s|s|g' build_tools/make_xml_documentation # fix sed command in this script to remove the backslash for BSD sed
		$AST_MAKE "ASTLDFLAGS=-lcrypt -lsysinfo" main
		if [ $? -eq 0 ]; then
			$AST_MAKE -j$(nproc) # compile Asterisk. This is the longest step, if you are installing for the first time. Also, don't let it take over the server.
		fi
	else
		$AST_MAKE -j$(nproc) main # compile 'main' subdirectory first
		if [ $? -eq 0 ]; then
			$AST_MAKE -j$(nproc) # compile Asterisk. This is the longest step, if you are installing for the first time. Also, don't let it take over the server.
		fi
	fi

	if [ $? -ne 0 ]; then
		gcc -v
		$AST_MAKE # Finish compiling antyhing that would build successfully, from the parallel build, so the noisy build only builds the offending target
		$AST_MAKE NOISY_BUILD=1 # show actual compilation command that failed, with no parallelism
		#if [ ! -f channels/chan_dahdi.o ]; then
		#	echoerr "Compilation of chan_dahdi failed?" # Only suggest this if we got around to compiling some channel drivers to begin with
		#	ls -la /usr/include/dahdi
		#	# Debug failed chan_dahdi compilation
		#	# chan_dahdi.c:7677:18: error: unused variable 'x' [-Werror=unused-variable]
		#	# 7677 |         int res, x;
		#	sed -n 7677,7800p channels/chan_dahdi.c
		#fi
		if [ "$DEVMODE" = "1" ] && [ -f doc/core-en_US.xml ]; then # run just make validate-docs for doc validation
			$XMLSTARLET val -d doc/appdocsxml.dtd -e doc/core-en_US.xml # by default, it doesn't tell you whether the docs failed to validate. So if validation failed, print that out.
		fi
		echoerr "Compilation or doc validation failed"
		exit 2
	fi
	if [ "$DEVMODE" = "1" ]; then
		$AST_MAKE full # some XML syntax warnings aren't shown unless make full is run
	fi
	if [ $? -ne 0 ]; then
		if [ "$DEVMODE" = "1" ]; then
			$XMLSTARLET val -d doc/appdocsxml.dtd -e doc/core-en_US.xml # by default, it doesn't tell you whether the docs failed to validate. So if validation failed, print that out.
		fi
		echoerr "Compilation or doc validation failed"
		exit 2
	fi

	# Install Asterisk
	if [ -d /usr/lib/asterisk/modules ]; then
		rm -f /usr/lib/asterisk/modules/*.so
	elif [ -d /usr/local/lib/asterisk/modules ]; then
		rm -f /usr/local/lib/asterisk/modules/*.so
	fi
	$AST_MAKE install # actually install modules and binary

	# Debugging: see where Asterisk got installed
	which asterisk
	which rasterisk
	if [ ! -f /usr/sbin/asterisk ] && [ ! -f /sbin/asterisk ] && [ ! -f /usr/local/sbin/asterisk ]; then
		echoerr "Could not find asterisk in either /usr/sbin or /sbin or /usr/local/sbin?"
		ls -la /usr/sbin/asterisk
		ls -la /sbin/asterisk
		ls -la /usr/local/sbin/asterisk
		exit 1
	fi

	if [ "$DEVMODE" = "1" ]; then
		$AST_MAKE install-headers # install development headers
	fi
	install_samples
	if [ "$OS_DIST_INFO" = "FreeBSD" ]; then
		for FILE in $(find /usr/local/lib/asterisk/modules -name "*.so" ) ; do
			nm -D ${FILE} | grep PJ_AF_INET
		done
	fi
	$AST_MAKE config # install init script (to run Asterisk as a service)
	ldconfig # update shared libraries cache
	if [ "$OS_DIST_INFO" = "FreeBSD" ]; then
		wget https://raw.githubusercontent.com/freebsd/freebsd-ports/7abe6caf38ac7e552642372be9b4136c4354d0fd/net/asterisk18/files/asterisk.in
		cp asterisk.in /usr/local/etc/rc.d/asterisk
		chmod +x /usr/local/etc/rc.d/asterisk
	fi
	$AST_MAKE install-logrotate # auto compress and rotate log files

	if [ "$ODBC" = "1" ]; then # MariaDB ODBC for Asterisk
		install_odbc
	fi

	if [ ${#AST_USER} -gt 0 ]; then
		id -u "$AST_USER"
		# Create a user, if needed
		if [ $? -ne 0 ]; then
			adduser -c "Asterisk" $AST_USER --disabled-password --gecos "" # don't allow any password logins, e.g. su - asterisk. Use passwd asterisk to manually set.
		fi
		sed -i "s/ASTARGS=\"\"/ASTARGS=\"-U $AST_USER\"/g" /sbin/safe_asterisk
		sed -i "s/#AST_USER=\"asterisk\"/AST_USER=\"$AST_USER\"/g" /etc/default/asterisk
		sed -i "s/#AST_GROUP=\"asterisk\"/AST_GROUP=\"$AST_USER\"/g" /etc/default/asterisk
		chown -R $AST_USER $AST_CONFIG_DIR/ /usr/lib/asterisk /var/spool/asterisk/ $AST_VARLIB_DIR/ /var/run/asterisk/ /var/log/asterisk /var/cache/asterisk /usr/sbin/asterisk
		sed -i "s/create 640 root root/create 640 $AST_USER $AST_USER/g" /etc/logrotate.d/asterisk # by default, logrotate will make the files owned by root, so Asterisk can't write to them if it runs as non-root user, so fix this! Not much else that can be done, as it's not a bug, since Asterisk itself doesn't necessarily know what user Asterisk will run as at compile/install time.
		# by default, it has the asterisk user, so simply uncomment it:
		sed -i 's/;runuser =/runuser =/' $AST_CONFIG_DIR/asterisk.conf
		sed -i 's/;rungroup =/rungroup =/' $AST_CONFIG_DIR/asterisk.conf
		if [ "${AST_USER}" != "asterisk" ]; then # but if we're not actually running as "asterisk", change that:
			sed -i "s/runuser = asterisk/runuser = $AST_USER/gw tmpuserchanges.txt" $AST_CONFIG_DIR/asterisk.conf
			if [ ! -s tmpuserchanges.txt ]; then
				echoerr "runuser option not detected in asterisk.conf"
			fi
			sed -i "s/rungroup = asterisk/rungroup = $AST_USER/gw tmpuserchanges.txt" $AST_CONFIG_DIR/asterisk.conf
			if [ ! -s tmpuserchanges.txt ]; then
				echoerr "rungroup option not detected in asterisk.conf"
			fi
			rm tmpuserchanges.txt
		fi
		chgrp $AST_USER $AST_VARLIB_DIR/astdb.sqlite3
		if [ -d /etc/dahdi ]; then
			# DAHDI related permissions: https://support.digium.com/s/article/Automatically-setting-dev-dahdi-file-permissions-when-running-Asterisk-as-non-root-user
			chown -R $AST_USER:$AST_USER /dev/dahdi
			if [ "${AST_USER}" != "asterisk" ]; then # if we're not actually running as "asterisk"
				sed -i "s/OWNER=\"asterisk\"/OWNER=\"$AST_USER\"/g" /etc/udev/rules.d/dahdi.rules
				sed -i "s/GROUP=\"asterisk\"/GROUP=\"$AST_USER\"/g" /etc/udev/rules.d/dahdi.rules
			fi
		fi
		# If we're using Let's Encrypt for our cert, then Asterisk needs to be able to read it. Otherwise, SIP/PJSIP will be very unhappy when they start.
		if [ -d /etc/letsencrypt/live/ ]; then
			chmod -R 740 /etc/letsencrypt/live/
			chmod -R 740 /etc/letsencrypt/archive/
			# Make the relevant letsencrypt folders owned by said group.
			chgrp -R $AST_USER /etc/letsencrypt/live
			chgrp -R $AST_USER /etc/letsencrypt/archive
		fi
	fi

	# Allow core dumps
	ulimit -c unlimited

	# Make sure we can read any keys we need, or Asterisk will not be happy when it restarts.
	if [ -d /etc/letsencrypt ]; then
		make_keys_readable
	fi

	if [ "$CHAN_SCCP" = "1" ]; then
		cd $AST_SOURCE_PARENT_DIR
		cd chan-sccp
		if [ $? -eq 0 ]; then
			git fetch
			git pull
			if [ $AST_MAJOR_VER -ge 21 ]; then
				# Remove macros, or it won't even compile
				$WGET "https://github.com/chan-sccp/chan-sccp/commit/3c90b6447b17639c52b47ed61cfb154b15ee84ec.patch"
				git apply "3c90b6447b17639c52b47ed61cfb154b15ee84ec.patch"
			fi
			./configure --enable-conference --enable-advanced-functions --with-asterisk=../$AST_SRC_DIR
			$AST_MAKE -j$(nproc) && $AST_MAKE install && $AST_MAKE reload
			if [ $? -ne 0 ]; then
				echoerr "Failed to install chan_sccp"
				exit 2
			fi
		fi
	fi

	# Development Mode (Asterisk Test Suite)
	if [ "$DEVMODE" = "1" ]; then
		if [ "$PAC_MAN" = "apt-get" ]; then
			apt-get install -y gdb # for astcoredumper
		fi
	fi

	if [ "$TEST_SUITE" = "1" ]; then
		install_testsuite_itself
	fi

	/etc/init.d/asterisk status
	/etc/init.d/asterisk start # service asterisk start
	/etc/init.d/asterisk status

	asterisk -V
	rasterisk -x "core show version"
	echo $?

	printf "%s\n" "Asterisk installation has completed. You may now connect to the Asterisk CLI: asterisk -r"
	printf "%s\n" "If you upgraded Asterisk, you will need to run 'core restart now' for the new version to load."
	if [ "$CHAN_DAHDI" = "1" ]; then
		echog "Note that DAHDI was installed and requires a reboot before it can be used."
		echog "Note that you will need to manually configure /etc/dahdi/system.conf appropriately for your spans."
	fi
	if [ "$FREEPBX_GUI" = "1" ]; then
		printf "%s\n" "Installation of FreePBX GUI will begin in 5 seconds..."
		sleep 5 # give time to read the message above, if we're watching...
		install_freepbx
	fi
	if [ "$PKG_AUDIT" = "1" ]; then
		if [ "$PAC_MAN" = "apt-get" ]; then
			pkg_after=$( apt list --installed )
		else
			echoerr "Distro not supported for package audit"
		fi
		printf "%s\n" "Package Audit: Before/After"
		# diff wants a file...
		printf "%s" "$pkg_before" > /tmp/phreaknet_audit_before.txt
		printf "%s" "$pkg_after" > /tmp/phreaknet_audit_after.txt
		diff /tmp/phreaknet_audit_before.txt /tmp/phreaknet_audit_after.txt
		rm /tmp/phreaknet_audit_before.txt /tmp/phreaknet_audit_after.txt
	fi
elif [ "$cmd" = "freepbx" ]; then
	install_freepbx_checks
	if [ "$AST_USER" = "asterisk" ]; then
		echoerr "Assuming that Asterisk runs as user 'asterisk'..."
		sleep 2
	fi
	install_freepbx
elif [ "$cmd" = "dahdi" ]; then
	assert_root
	CHAN_DAHDI=1 # This is used by the pre-req install process to install DAHDI-specific prereqs.
	install_prereq 0 # Install basic build requirements
	install_dahdi
elif [ "$cmd" = "wanpipe" ]; then
	assert_root
	install_wanpipe
elif [ "$cmd" = "odbc" ]; then
	assert_root
	install_odbc
elif [ "$cmd" = "installts" ]; then
	assert_root
	install_testsuite "$FORCE_INSTALL"
elif [ "$cmd" = "uninstall" ]; then
	assert_root
	cd $AST_SOURCE_PARENT_DIR
	AST_SRC_DIR=`get_newest_astdir`
	if [ -f $AST_CONFIG_DIR/freepbx_module_admin.conf ]; then
		uninstall_freepbx
	fi
	cd $AST_SRC_DIR
	$AST_MAKE uninstall
elif [ "$cmd" = "uninstall-all" ]; then
	assert_root
	cd $AST_SOURCE_PARENT_DIR
	AST_SRC_DIR=`get_newest_astdir`
	echoerr "WARNING: This will remove all configuration, spool contents, logs, etc."
	if [ "$FORCE_INSTALL" != "1" ]; then
		sleep 3
	fi
	if [ -f $AST_CONFIG_DIR/freepbx_module_admin.conf ]; then
		uninstall_freepbx
	fi
	cd $AST_SRC_DIR
	$AST_MAKE uninstall-all
elif [ "$cmd" = "pulsar" ]; then
	assert_root
	require_installed_asterisk
	cd $AST_SOURCE_PARENT_DIR
	# Certificate has expired (unmaintained)
	wget --no-check-certificate https://octothorpe.info/downloads/pulsar-agi.tar.gz
	wget https://code.phreaknet.org/asterisk/pulsar-noanswer.agi # bug fix to pulsar.agi, to fix broken answer supervision:
	mv pulsar-agi.tar.gz $AST_VARLIB_DIR
	cd $AST_VARLIB_DIR
	tar xvfz pulsar-agi.tar.gz # automatically creates $AST_VARLIB_DIR/sounds/pulsar/
	mv $AST_SOURCE_PARENT_DIR/pulsar-noanswer.agi $AST_VARLIB_DIR/agi-bin/pulsar.agi
	chmod 755 $AST_VARLIB_DIR/agi-bin/pulsar.agi
	if [ ! -f $AST_VARLIB_DIR/agi-bin/pulsar.agi ]; then
		echoerr "pulsar.agi is missing"
	fi
	rm pulsar-agi.tar.gz
elif [ "$cmd" = "sounds" ]; then
	assert_root
	require_installed_asterisk
	printf "%s\n" "Installing Asterisk audio files..."

	printf "%s\n" "Installing Pat Fleet audio files..."
	cd /tmp
	mkdir -p patfleet
	cd patfleet
	git clone https://github.com/hharte/PatFleet-asterisk/
	cd PatFleet-asterisk/pa
	mv dictate/* $AST_SOUNDS_DIR/dictate
	mv digits/* $AST_SOUNDS_DIR/digits
	mv followme/* $AST_SOUNDS_DIR/followme
	mv letters/* $AST_SOUNDS_DIR/letters
	mv phonetic/* $AST_SOUNDS_DIR/phonetic
	mv silence/* $AST_SOUNDS_DIR/silence
	mv *.ulaw $AST_SOUNDS_DIR
	rm -rf /tmp/patfleet

	if [ "$BOILERPLATE_SOUNDS" = "1" ]; then
		install_boilerplate_sounds
	fi
elif [ "$cmd" = "boilerplate-sounds" ]; then
	install_boilerplate_sounds
elif [ "$cmd" = "ulaw" ]; then
	ensure_installed sox
	if [ ${#2} -gt 0 ]; then # a specific file
		withoutextension=`printf '%s\n' "$2" | sed -r 's|^(.*?)\.\w+$|\1|'` # see comment on https://stackoverflow.com/a/26753382
		sox "$2" --rate 8000 --channels 1 --type ul $withoutextension.ulaw lowpass 3400 highpass 300
	else
		for f in *.wav; do
			withoutextension=`printf '%s\n' "$f" | sed -r 's|^(.*?)\.\w+$|\1|'`
			sox $f --rate 8000 --channels 1 --type ul $withoutextension.ulaw lowpass 3400 highpass 300
		done
	fi
elif [ "$cmd" = "remsil" ]; then
	ensure_installed sox
	# if there is only 1 file, sox -m will fail, ignore.
	if [ ${#2} -gt 0 ]; then # a specific file
		f="$2"
		withoutextension=`printf '%s' "$f" | sed -r 's|^(.*?)\.\w+$|\1|'`
		withoutextensionplus="${withoutextension}_"
		sox "$f" "remsil-$withoutextensionplus.wav" silence 1 1.5 0.1% 1 1.5 0.1% : newfile : restart # individual files
		#echo sox -m remsil-$withoutextensionplus*.wav "remsil-$withoutextensionplus.wav" # combined file
		#ls remsil-$withoutextensionplus*.wav
		#sox -m remsil-$withoutextensionplus*.wav "remsil-$withoutextensionplus.wav"
		# XXX: For some reason, this (and above) doesn't work right, it can/will only merge some of the files, instead of all of them, making this useless
		sox -m $(for ff in remsil-$withoutextensionplus*.wav; do echo -n "$ff "; done) "remsil-$withoutextensionplus.wav"
	else
		for f in *.wav; do
			withoutextension=`printf '%s' "$f" | sed -r 's|^(.*?)\.\w+$|\1|'`
			withoutextensionplus="${withoutextension}_"
			sox "$f" "remsil-$withoutextensionplus.wav" silence 1 1.5 0.1% 1 1.5 0.1% : newfile : restart
			sox -m remsil-$withoutextensionplus*.wav "remsil-$withoutextensionplus.wav" 2>&1 >/dev/null
		done
	fi
elif [ "$cmd" = "docverify" ]; then
	$XMLSTARLET val -d doc/appdocsxml.dtd -e doc/core-en_US.xml # show the XML validation errors
	$XMLSTARLET val -d doc/appdocsxml.dtd -e doc/core-en_US.xml 2>&1 | grep "doc/core-en_US.xml:" | cut -d':' -f2 | cut -d'.' -f1 | xargs -d "\n" -I{} sed "{}q;d" doc/core-en_US.xml # show the offending lines
elif [ "$cmd" = "fullpatch" ]; then
	cd $AST_SOURCE_PARENT_DIR
	AST_SRC_DIR=`get_newest_astdir`
	if [ $? -ne 0 ]; then
		die "Asterisk not currently installed?"
	fi
	read -r -p "Source File: " filename
	# ${var:i:j} substring expansion not available in POSIX sh
	if [ "$(expr substr "$filename" 1 4)" = "app_" ]; then
		filename="apps/${filename}"
	elif [ "$(expr substr "$filename" 1 5)" = "func_" ]; then
		filename="funcs/${filename}"
	elif [ "$(expr substr "$filename" 1 4)" = "res_" ]; then
		filename="res/${filename}"
	fi
	length=$(expr length "${filename}")
	clen=$(( $length - 1 )) # 1 not 2, since we're 1-indexed
	if [ $clen -gt 0 ]; then
		if [ "$(expr substr "$filename" "$clen" 2)" != ".c" ]; then
			printf "Auto appending .c file extension\n"
			filename="${filename}.c"
		fi
	fi
	instantiate_repo
	phreak_tree_module "$filename"
elif [ "$cmd" = "runtest" ]; then
	if [ ${#2} -eq 0 ]; then
		echoerr "Missing argument."
		exit 2
	fi
	run_testsuite_test_only "$2" 0
elif [ "$cmd" = "stresstest" ]; then
	if [ ${#2} -eq 0 ]; then
		echoerr "Missing argument."
		exit 2
	fi
	run_testsuite_test_only "$2" 1
elif [ "$cmd" = "runtests" ]; then
	run_testsuite_tests
elif [ "$cmd" = "docgen" ]; then
	echoerr "docgen is deprecated. Please migrate to mkdocs instead."
	sleep 1
	cd $AST_SOURCE_PARENT_DIR
	AST_SRC_DIR=`get_newest_astdir`
	cd $AST_SRC_DIR
	if [ $? -ne 0 ]; then
		echoerr "Couldn't find Asterisk source directory?"
		exit 2
	fi
	if [ ! -f doc/core-en_US.xml ]; then
		printf "%s\n" "Failed to find any XML documentation. Has Asterisk been installed yet?"
		exit 2
	fi
	$XMLSTARLET val -d doc/appdocsxml.dtd -e doc/core-en_US.xml
	if [ $? -ne 0 ]; then
		exit 2 # if the XML docs aren't valid, then give up now
	fi
	if [ "$PAC_MAN" = "apt-get" ]; then
		apt-get install -y php php8.2-xml # XXX TODO: Should determine correct version dynamically
	fi
	printf "%s\n" "Obtaining latest version of astdocgen..."
	wget -q "https://raw.githubusercontent.com/InterLinked1/astdocgen/master/astdocgen.php" -O astdocgen.php --no-cache
	chmod +x astdocgen.php
	./astdocgen.php -f doc/core-en_US.xml -x -s > /tmp/astdocgen.xml
	if [ $? -ne 0 ]; then
		echoerr "Failed to generate XML dump"
		exit 2
	fi
	./astdocgen.php -f /tmp/astdocgen.xml -h > doc/index.html
	if [ $? -ne 0 ]; then
		echoerr "Failed to generate HTML documentation"
		exit 2
	fi
	rm /tmp/astdocgen.xml
	printf "HTML documentation has been generated and is now saved to %s%s\n" "$AST_SOURCE_PARENT_DIR/$AST_SRC_DIR" "doc/index.html"
	# To remove any packages installed purely for astdocgen:
	# apt-get remove apache2 apache2-bin apache2-data apache2-utils libapache2-mod-php8.2 libaprutil1-dbd-sqlite3 libaprutil1-ldap liblua5.3-0 php php8.2 php8.2-xml
elif [ "$cmd" = "mkdocs" ]; then
	cd $AST_SOURCE_PARENT_DIR
	AST_SRC_DIR=`get_newest_astdir`
	cd /tmp
	if [ -d documentation ]; then
		cd documentation
		git stash
		git pull
		rm -rf /tmp/documentation/temp/site
	else
		git clone https://github.com/asterisk/documentation.git --depth 1
		cd documentation
	fi

	pip3 install -r requirements.txt

	# Set up build
	echo "ASTERISK_XML_FILE := $AST_SOURCE_PARENT_DIR/$AST_SRC_DIR/doc/core-en_US.xml" > Makefile.inc
	echo "SKIP_ARI := yes" >> Makefile.inc
	echo "BRANCHES := master" >> Makefile.inc

	printf "Building documentation... this may take a couple minutes\n"

	$AST_MAKE BRANCH=master NO_STATIC=yes

	printf "Documentation has been built to %s\n" "/tmp/documentation/temp/site"
elif [ "$cmd" = "pubdocs" ]; then
	cd $AST_SOURCE_PARENT_DIR
	AST_SRC_DIR=`get_newest_astdir`
	apt-get install -y python-dev python-virtualenv python-lxml
	pip install pystache
	pip install premailer
	if [ -d publish-docs ]; then
		rm -rf publish-docs
	fi
	git clone https://github.com/asterisk/publish-docs.git
	cd publish-docs
	echo $AST_SOURCE_PARENT_DIR
	printf "%s\n" "Generating Confluence markup..."
	if [ ! -f "$AST_SOURCE_PARENT_DIR/$AST_SRC_DIR/doc/core-en_US.xml" ]; then
		echoerr "File does not exist: $AST_SOURCE_PARENT_DIR/$AST_SRC_DIR/doc/core-en_US.xml"
		ls -d -v $AST_SOURCE_PARENT_DIR/*/ | grep "asterisk-" | tail -1 ###### not FreeBSD compatible
		exit 2
	fi
	./astxml2wiki.py --username=wikibot --server=https://wiki.asterisk.org/wiki/rpc/xmlrpc '--prefix=Asterisk 18' --space=AST --file=$AST_SOURCE_PARENT_DIR/$AST_SRC_DIR/doc/core-en_US.xml --password=d --debug > confluence.txt
	printf "%s\n" "Converting Confluence to HTML..."
	if [ ! -f confluence2html ]; then
		wget https://raw.githubusercontent.com/rmloveland/confluence2html/master/confluence2html
	fi
	chmod +x confluence2html
	PERL_MM_USE_DEFAULT=1 cpan -i CPAN
	perl -MCPAN -e 'install File::Slurp'
	./confluence2html < confluence.txt > docs.html
	ls -l $AST_SOURCE_PARENT_DIR/publish-docs
	printf "%s\n" "All Wiki documentation has been generated and is now in $AST_SOURCE_PARENT_DIR/publish-docs/docs.html"
	printf "%s\n" "phreaknet pubdocs is deprecated. Please migrate to phreaknet mkdocs instead"
elif [ "$cmd" = "config" ]; then
	inject=1
	cd $AST_SOURCE_PARENT_DIR
	AST_SRC_DIR=`get_newest_astdir`
	cd $AST_SRC_DIR
	if [ $? -ne 0 ]; then
		die "Asterisk configuration directory does not exist"
	fi
	install_samples
	if [ ${#INTERLINKED_APIKEY} -eq 0 ]; then
		printf '\a'
		read -r -p "InterLinked API key: " INTERLINKED_APIKEY
	fi
	if [ ${#PHREAKNET_CLLI} -eq 0 ]; then
		printf '\a'
		read -r -p "PhreakNet CLLI code: " PHREAKNET_CLLI
	fi
	if [ ${#PHREAKNET_DISA} -eq 0 ]; then
		printf '\a'
		read -r -p "PhreakNet DISA number: " PHREAKNET_DISA
	fi
	if [ ${#INTERLINKED_APIKEY} -lt 30 ]; then
		echoerr "InterLinked API key must be greater than 30 characters."
		inject=0
	fi
	if [ ${#PHREAKNET_DISA} -ne 7 ]; then
		echoerr "PhreakNet DISA number must be 7 digits."
		inject=0
	fi
	printf "%s: %s\n" "PhreakNet CLLI code" $PHREAKNET_CLLI
	printf "%s\n" "InterLinked API key seems to be of valid format, not displaying for security reasons..."
	install_boilerplate
	if [ "$inject" = "1" ]; then
		## Inject user config (CLLI code, API key)
		sed -i "s/abcdefghijklmnopqrstuvwxyz/$INTERLINKED_APIKEY/g" $AST_CONFIG_DIR/$EXTENSIONS_CONF_FILE
		sed -i "s/WWWWXXYYZZZ/$PHREAKNET_CLLI/g" $AST_CONFIG_DIR/$EXTENSIONS_CONF_FILE
		sed -i "s/5551111/$PHREAKNET_DISA/g" $AST_CONFIG_DIR/$EXTENSIONS_CONF_FILE
		printf "Updated [globals] in %s/extensions.conf with dynamic variables. If globals are stored in a different file, manual updating is required." $AST_CONFIG_DIR
		## Also update verify.conf
		sed -i "s/HSTNTXMOCG0/$PHREAKNET_CLLI/g" $AST_CONFIG_DIR/verify.conf
		sed -i "s/5551111/$PHREAKNET_DISA/g" $AST_CONFIG_DIR/verify.conf
		printf "Updated %s/verify.conf" $AST_CONFIG_DIR
	fi
	printf "%s\n" "Boilerplate config installed! Note that these files may still require manual editing before use."
elif [ "$cmd" = "bconfig" ]; then
	cd $AST_SOURCE_PARENT_DIR
	AST_SRC_DIR=`get_newest_astdir`
	cd $AST_SRC_DIR
	if [ $? -ne 0 ]; then
		die "Asterisk configuration directory does not exist"
	fi
	install_samples
	install_boilerplate
elif [ "$cmd" = "keygen" ]; then
	if [ ${#INTERLINKED_APIKEY} -eq 0 ]; then
		INTERLINKED_APIKEY=`grep -R "interlinkedkey=" $AST_CONFIG_DIR | grep -v "your-interlinked-api-key" | cut -d'=' -f2 | awk '{print $1;}'`
		if [ ${#INTERLINKED_APIKEY} -lt 30 ]; then
			echoerr "Failed to find InterLinked API key. For future use, please set your [globals] variables, e.g. by running phreaknet config"
			printf '\a'
			read -r -p "InterLinked API key: " INTERLINKED_APIKEY
			if [ ${#INTERLINKED_APIKEY} -lt 30 ]; then
				echoerr "InterLinked API key too short. Not going to attempt uploading the public key..."
			fi
		fi
	fi
	if [ ${#PHREAKNET_CLLI} -eq 0 ]; then
		PHREAKNET_CLLI=`grep -R "clli=" $AST_CONFIG_DIR | grep -v "5551111" | grep -v "curl " | grep -v "<switch-clli>" | cut -d'=' -f2 | awk '{print $1;}'`
		if [ ${#PHREAKNET_CLLI} -ne 11 ]; then
			printf "Failed to find CLLI by grepping configs, querying Asterisk...\n"
			PHREAKNET_CLLI=`/sbin/rasterisk -x "dialplan show globals" | grep "clli=" | cut -d'=' -f2 | awk '{print $1;}'`
		fi
		if [ ${#PHREAKNET_CLLI} -ne 11 ]; then
			echoerr "Failed to find PhreakNet CLLI. For future use, please set your [globals] variables, e.g. by running phreaknet config"
			printf '\a'
			read -r -p "PhreakNet CLLI: " PHREAKNET_CLLI
			if [ ${#PHREAKNET_CLLI} -ne 11 ]; then
				echoerr "PhreakNet CLLI must be 11 characters. Not going to attempt uploading the public key..."
			fi
		fi
	fi
	cd $AST_VARLIB_DIR/keys/
	if [ "$ASTKEYGEN" = "1" ]; then
		if [ -f phreaknetrsa.key ]; then
			printf "%s\n" "Rotating keys..."
		else
			printf "%s\n" "Initializing keys..."
		fi
		astgenkey -q -n phreaknetrsa
	else
		printf "%s\n" "--rotate flag not provided, skipping key rotation or creation."
	fi
	if [ ! -f $AST_VARLIB_DIR/keys/phreaknetrsa.pub ]; then
		die "File phreaknetrsa.pub not found"
	fi
	keylen=`wc -c $AST_VARLIB_DIR/keys/phreaknetrsa.pub | awk '{print $1;}'`
	if [ "$keylen" = "" ]; then
		echoerr "File %s not readable?" "$AST_VARLIB_DIR/keys/phreaknetrsa.pub"
	elif [ $keylen -le 50 ]; then
		echoerr "Readable key length only %s?" "$keylen" # if file not readable for some reason, probably won't work
	else
		printf "Key length: %s\n" "$keylen"
	fi
	cat phreaknetrsa.pub
	if [ ${#INTERLINKED_APIKEY} -gt 30 ] && [ ${#PHREAKNET_CLLI} -eq 11 ]; then
		wanip=`dig +short myip.opendns.com @resolver4.opendns.com`
		printf "%s %s\n" "Autodetected WAN IP address is" $wanip
		wanfqdn=`dig +short -x $wanip`
		if [ "$wanfqdn" = "" ]; then
			printf "Could not autodetect FQDN\n"
		else
			printf "%s %s\n" "Autodetected WAN FQDN hostname is" "$wanfqdn"
		fi
		printf "%s\n" "Attempting to upload RSA public key..."
		# if curl version is >= 7.76, we could use --fail-with-body and make just one request
		curl --fail -X POST --data-urlencode "rsakey=$(cat phreaknetrsa.pub)" -d "key=$INTERLINKED_APIKEY" -d "clli=$PHREAKNET_CLLI" https://api.phreaknet.org/v1/rsa
		if [ $? -ne 0 ]; then
			echoerr "RSA key upload of $AST_VARLIB_DIR/keys/phreaknetrsa.pub failed."
			# Repeat, without --fail, for error message:
			curl -X POST --data-urlencode "rsakey=$(cat phreaknetrsa.pub)" -d "key=$INTERLINKED_APIKEY" -d "clli=$PHREAKNET_CLLI" https://api.phreaknet.org/v1/rsa
			printf "\n"
			exit 1
		fi
		printf "\n"
	else
		printf "%s\n" "No InterLinked API key and/or CLLI on file, skipping upload of public key..."
	fi
	touch $AST_CONFIG_DIR/iax-phreaknet-rsa-in.conf
	# touch $AST_CONFIG_DIR/iax-phreaknet-rsa-out.conf # no longer necessary for >= 18.9
	# Determine what user Asterisk is running as.
	# cut on ; before = so that we properly handled commented out lines.
	astrunuser=$( grep "runuser" $AST_CONFIG_DIR/asterisk.conf | cut -d';' -f1 | cut -d'=' -f2 | xargs | tr -d '\n' )
	if [ ${#astrunuser} -gt 0 ]; then
		## If you are running Asterisk as non-root, make the user as which Asterisk runs own the private key and the new files:
		chown "$astrunuser" phreaknetrsa.key
		chown "$astrunuser" phreaknetrsa.pub
		chown "$astrunuser" $AST_CONFIG_DIR/iax.conf
		chown "$astrunuser" $AST_CONFIG_DIR/iax-phreaknet*
	fi
	asterisk -rx "module reload res_crypto"
	asterisk -rx "keys init"
	asterisk -rx "keys show"
elif [ "$cmd" = "keyperms" ]; then
	make_keys_readable
elif [ "$cmd" = "fail2ban" ]; then
	ensure_installed iptables
	ensure_installed fail2ban
	if [ -f /etc/fail2ban/filter.d/asterisk.conf ]; then
		die "Existing fail2ban configuration for Asterisk already found, exiting..."
	fi
	wget -q "https://raw.githubusercontent.com/fail2ban/fail2ban/master/config/filter.d/asterisk.conf" -O /etc/fail2ban/filter.d/asterisk.conf
elif [ "$cmd" = "ban" ]; then
	if [ ${#2} -lt 1 ]; then
		die "Must specify an IP address or CIDR range"
	fi
	iptables -A INPUT -s $2 -j DROP
elif [ "$cmd" = "update" ]; then
	assert_root
	if [ ! -f "/tmp/phreakscript_update.sh" ] || [ $(stat -c%s "/tmp/phreakscript_update.sh") -eq 0 ]; then
		wget -q "https://raw.githubusercontent.com/InterLinked1/phreakscript/master/phreakscript_update.sh" -O /tmp/phreakscript_update.sh
		chmod +x /tmp/phreakscript_update.sh
	fi
	printf "%s\n" "Updating PhreakScript..."
	if [ "$PAC_MAN" = "apt-get" ]; then
		if [ ! -d /etc/bash_completion.d ]; then
			apt-get install -y bash-completion
		fi
	fi
	if [ -d /etc/bash_completion.d ]; then
		if [ -f /tmp/phreakscript_autocomplete.sh ]; then
			rm /tmp/phreakscript_autocomplete.sh
		fi
		if [ ! -f /etc/bash_completion.d/phreaknet ]; then # try to add auto-completion
			if [ -f /etc/bash.bashrc ]; then
				printf "Downloading auto-completion binding script\n"
				wget -q "https://raw.githubusercontent.com/InterLinked1/phreakscript/master/phreakscript_autocomplete.sh" -O /tmp/phreakscript_autocomplete.sh
				mv /tmp/phreakscript_autocomplete.sh /etc/bash_completion.d/phreaknet
				# by default, auto completion isn't active, so activate it:
				# there has GOT to be a much better way to do this, but technically this will work:
				sed -i -r "s|#if ! shopt -oq posix; then|if ! shopt -oq posix; then|g" /etc/bash.bashrc
				sed -i -r "s|#  if|  if|g" /etc/bash.bashrc
				sed -i -r "s|#    . /usr/share/bash-completion/bash_completion|    . /usr/share/bash-completion/bash_completion|g" /etc/bash.bashrc
				sed -i -r "s|#  elif|  elif|g" /etc/bash.bashrc
				sed -i -r "s|#    . /etc/bash_completion|    . /etc/bash_completion|g" /etc/bash.bashrc
				sed -i -r "s|#  fi|  fi|g" /etc/bash.bashrc
				sed -i -r "s|#fi|fi|g" /etc/bash.bashrc
			fi
		fi
	fi
	exec /tmp/phreakscript_update.sh "$SCRIPT_UPSTREAM" "$FILE_PATH" "/tmp/phreakscript_update.sh"
elif [ "$cmd" = "astpr" ]; then
	if [ "$2" = "" ]; then
		die "astpr requires a PR number"
	fi
	asterisk_pr "$2" 1 # force patch
elif [ "$cmd" = "patch" ]; then
	printf "%s\n" "Updating PhreakNet Asterisk configuration..."
	rm -f /tmp/phreakpatch/*.patch
	rm -rf /tmp/phreakpatch
	mkdir /tmp/phreakpatch
	printf "%s\n" "Downloading patches..."
	$WGET -r -l 1 -nd -np -q -P /tmp/phreakpatch -R -A patch "https://code.phreaknet.org/asterisk/patches/"
	find /tmp/phreakpatch -name "*index*.html*" | xargs rm
	printf "%s\n" "Patching configuration..."
	if [ ! -d $AST_CONFIG_DIR/dialplan/phreaknet ]; then
		mkdir -p $AST_CONFIG_DIR/dialplan/phreaknet
	fi
	touch $AST_CONFIG_DIR/dialplan/phreaknet/patches.lst
	for file in /tmp/phreakpatch/*; do
		if [ -f $file ]; then
			base=`basename $file`
			if grep -q "$base" $AST_CONFIG_DIR/dialplan/phreaknet/patches.lst; then
				printf "%s\n" "Patch $base already installed or rejected, not applying again..."
				continue
			fi
			endfile=`printf '%s' $base | cut -d'_' -f2 | cut -d'.' -f1-2`
			if [ -f "$AST_CONFIG_DIR/dialplan/phreaknet/$endfile" ]; then
				patch -u -b -N $AST_CONFIG_DIR/dialplan/phreaknet/$endfile -i $file
			elif [ -f "$AST_CONFIG_DIR/dialplan/phreaknet/$endfile" ]; then
				patch -u -b -N $AST_CONFIG_DIR/dialplan/phreaknet/$endfile -i $file
			elif [ -f "$AST_CONFIG_DIR/dialplan/phreaknet/$endfile" ]; then
				patch -u -b -N $AST_CONFIG_DIR/dialplan/phreaknet/$endfile -i $file
			else
				echoerr "Failed to apply patch $file, couldn't find $endfile!"
				continue
			fi
			echo "$base" >> $AST_CONFIG_DIR/dialplan/phreaknet/patches.lst
		else
			echoerr "No file to apply?"
		fi
	done
	rm -rf /tmp/phreakpatch
	printf "%s\n" "Patching complete!"
elif [ "$cmd" = "genpatch" ]; then
	echog "PhreakPatch Generation Helper Utility v1"
	cd /tmp
	printf '\a'
	read -r -p "Full path to file that will be patched idempotently: " file1
	if [ ! -f $file1 ]; then
		echoerr "File $file1 does not exist!"
		exit 2
	fi
	base=`basename $file1`
	mkdir /tmp/patch_${EPOCHSECONDS}_0
	mkdir /tmp/patch_${EPOCHSECONDS}_1
	old="/tmp/patch_${EPOCHSECONDS}_0/${base}"
	new="/tmp/patch_${EPOCHSECONDS}_1/${base}"
	patchfile="/tmp/${EPOCHSECONDS}_${base}.patch"
	cp $file1 $old
	printf '\a'
	read -r -p "Now patch the file and press ENTER when done: "
	cp $file1 $new
	diff -u $old $new > $patchfile
	rm -f $old $new /tmp/patch_${EPOCHSECONDS}_0 /tmp/patch_${EPOCHSECONDS}_1
	printf "%s\n" "Patch file generated! Saved as $patchfile"
	if [ -s $patchfile ]; then
		read -e -i "$DEFAULT_PATCH_DIR" -p "Provide a folder for copy or press ENTER to finish: " input
		newfolder="${input:-newfolder}"
		if [ ${#newfolder} -gt 0 ]; then
			if [ -d $newfolder ]; then
				cp $patchfile $newfolder
				printf "%s\n" "Patch copied to $newfolder"
			else
				echoerr "Directory $newfolder doesn't exist. Patch not copied."
			fi
		fi
	else
		printf "%s\n" "Patch file is empty. Aborting."
	fi
elif [ "$cmd" = "alembic" ]; then
	# This should only be done in a Git repository (developer usage only)
	git status
	if [ $? -ne 0 ]; then
		die "Not currently in a Git directory?"
	fi
	# Go into the alembic directory
	cd contrib/ast-db-manage
	if [ $? -ne 0 ]; then
		die "Directory not found: contrib/ast-db-manage"
	fi
	read -r -p "Alembic title: " title
	alembic -c config.ini.sample revision -m "$title"
	# https://alembic.sqlalchemy.org/en/latest/tutorial.html#create-a-migration-script
	printf "%s\n" "You will now need to manually edit the created file."
	printf "%s\n" "If your branch is not up to date, be sure to also update the head revision number!"
elif [ "$cmd" = "kill" ]; then
	ast_kill
elif [ "$cmd" = "forcerestart" ]; then
	ast_kill
	sleep 1
	if ! asterisk -g; then
		echoerr "Failed to start Asterisk again."
	else
		echog "Successfully started Asterisk again."
	fi
elif [ "$cmd" = "restart" ]; then
	# not really forcing install of anything, but to use the --force flag
	if [ "$FORCE_INSTALL" = "1" ]; then
		restart_telephony 0
	else
		restart_telephony 1
	fi
elif [ "$cmd" = "stop" ]; then
	# not really forcing install of anything, but to use the --force flag
	if [ "$FORCE_INSTALL" = "1" ]; then
		stop_telephony 0
	else
		stop_telephony 1
	fi
elif [ "$cmd" = "start" ]; then
	start_telephony
elif [ "$cmd" = "edit" ]; then
	exec nano $FILE_PATH
elif [ "$cmd" = "touch" ]; then
	printf "%s: " "$FILE_PATH"
	date -r $FILE_PATH
elif [ "$cmd" = "validate" ]; then
	run_rules
elif [ "$cmd" = "trace" ]; then
	VERBOSE_LEVEL=10
	debugtime=$EPOCHSECONDS
	if [ "$debugtime" = "" ]; then
		debugtime=`awk 'BEGIN {srand(); print srand()}'` # https://stackoverflow.com/a/41324810
	fi
	if [ "$debugtime" = "" ]; then
		echoerr "No debug time?"
	fi
	channel="debug_$debugtime.txt"
	if [ "$DEBUG_LEVEL" = "" ]; then
		printf "%s\n" "Debug level defaulting to 0"
		DEBUG_LEVEL=0
	fi
	if [ "$DEBUG_LEVEL" -eq "$DEBUG_LEVEL" ] 2> /dev/null; then
        if [ $DEBUG_LEVEL -lt 0 ]; then
			echoerr "Invalid debug level: $DEBUG_LEVEL"
			exit 2
		elif [ $DEBUG_LEVEL -gt 10 ]; then
			echoerr "Invalid debug level: $DEBUG_LEVEL"
			exit 2
		else
			if [ $DEBUG_LEVEL -gt 0 ]; then
				asterisk -rx "logger add channel $channel notice,warning,error,verbose,debug,dtmf"
				asterisk -rx "core set debug $DEBUG_LEVEL"
			else
				asterisk -rx "logger add channel $channel notice,warning,error,verbose"
			fi
		fi
	else
		echoerr "Debug level must be numeric: $DEBUG_LEVEL"
		exit 2
	fi
	asterisk -rx "core set verbose $VERBOSE_LEVEL"
	asterisk -rx "iax2 set debug on" # this might not actually appear in the trace, but doesn't hurt...
	printf "Starting trace (verbose %d, debug %d): %s\n" "$VERBOSE_LEVEL" "$DEBUG_LEVEL" "$debugtime"
	printf "%s\n" "Starting CLI trace..."
	read -r -p "A CLI trace is now being collected. Reproduce the issue, then press ENTER to conclude the trace: " x
	printf "%s\n" "CLI trace terminated..."
	asterisk -rx "logger remove channel $channel"
	if [ ! -f /var/log/asterisk/$channel ]; then
		echoerr "CLI log trace file not found, aborting..."
		exit 2
	fi
	settings=`asterisk -rx 'core show settings'`
	printf "\n" >> /var/log/asterisk/$channel
	echo "------------------------------------------------------------------------" >> /var/log/asterisk/$channel
	echo "$settings" >> /var/log/asterisk/$channel # append helpful system info.
	echo "------------------------------------------------------------------------" >> /var/log/asterisk/$channel
	phreakscript_info >> /var/log/asterisk/$channel # append helpful system info.
	paste_post "/var/log/asterisk/$channel" "1" "1"
elif [ "$cmd" = "dialplanfiles" ]; then
	debugtime=$EPOCHSECONDS
	if [ "$debugtime" = "" ]; then
		debugtime=`awk 'BEGIN {srand(); print srand()}'` # https://stackoverflow.com/a/41324810
	fi
	if [ "$debugtime" = "" ]; then
		echoerr "No debug time?"
	fi
	channel="debug_$debugtime.txt"
	asterisk -rx "logger add channel $channel debug"
	asterisk -rx "core set debug 1"
	asterisk -rx "dialplan reload"
	asterisk -rx "core set debug 0"
	asterisk -rx "logger remove channel $channel"
	if [ ! -f /var/log/asterisk/$channel ]; then
		echoerr "CLI log trace file not found, aborting..."
		exit 2
	fi
	grep "config.c: Parsing " /var/log/asterisk/$channel
	rm /var/log/asterisk/$channel
elif [ "$cmd" = "applist" ]; then
	grep -ERo ",([A-Z][A-Za-z]+)\(" | cut -d',' -f2 | cut -d'(' -f1 | sort | uniq
elif [ "$cmd" = "funclist" ]; then
	grep -ERo "\{([A-Z]+)\(" | cut -d'{' -f2 | cut -d'(' -f1 | sort | uniq
elif [ "$cmd" = "paste" ]; then
	if [ ${#2} -eq 0 ]; then
		die "Usage: phreaknet paste <filename>"
	fi
	if [ ! -f "$2" ]; then
		die "File $2 does not exist"
	fi
	paste_post "$2"
elif [ "$cmd" = "enable-backtraces" ]; then
	cd $AST_SOURCE_PARENT_DIR
	AST_SRC_DIR=`get_newest_astdir`
	cd $AST_SRC_DIR
	$AST_MAKE menuselect.makeopts
	menuselect/menuselect --enable DONT_OPTIMIZE --enable BETTER_BACKTRACES menuselect.makeopts
	$AST_MAKE
	$AST_MAKE install
elif [ "$cmd" = "valgrind" ]; then # https://wiki.asterisk.org/wiki/display/AST/Valgrind
	apt-get install -y valgrind
	cd $AST_SOURCE_PARENT_DIR
	AST_SRC_DIR=`get_newest_astdir`
	cd /tmp
	asterisk -rx "core stop now"
	sleep 1
	valgrind --suppressions=$AST_SOURCE_PARENT_DIR/${AST_SRC_DIR}contrib/valgrind.supp -s --log-fd=9 asterisk -vvvvcg 9 > /tmp/asteriskvalgrind.txt
	paste_post "/tmp/asteriskvalgrind.txt"
	ls /tmp/asteriskvalgrind.txt
elif [ "$cmd" = "cppcheck" ]; then
	ensure_installed cppcheck
	cd_ast
	printf "%s\n" "Running cppcheck and dumping errors to cppcheck.txt"
	cppcheck --enable=all --suppress=unknownMacro . 2> cppcheck.txt
	printf "%s\n" "cppcheck completed. Errors dumped to cppcheck.txt"
elif [ "$cmd" = "malloc-debug" ]; then # https://wiki.asterisk.org/wiki/display/AST/MALLOC_DEBUG+Compiler+Flag
	cd $AST_SOURCE_PARENT_DIR
	AST_SRC_DIR=`get_newest_astdir`
	cd $AST_SRC_DIR
	asterisk -rx "core show settings" | grep "Build Options:" | grep "MALLOC_DEBUG"
	if [ $? -ne 0 ]; then # either Asterisk isn't running or MALLOC_DEBUG isn't a currently compiler option, so recompile with MALLOC_DEBUG
		$AST_MAKE menuselect.makeopts
		menuselect/menuselect --enable DONT_OPTIMIZE --enable BETTER_BACKTRACES --enable MALLOC_DEBUG menuselect.makeopts
		$AST_MAKE
		$AST_MAKE install
	fi
	cd /tmp
	read -r -p "Press ENTER to restart Asterisk with MALLOC_DEBUG enabled: "
	asterisk -rx "core stop now"
	asterisk -g
	read -r -p "MALLOC_DEBUG is currently collecting debug. Press ENTER once issue has been reproduced: "
	if [ ! -f "/var/log/asterisk/mmlog" ]; then
		die "Could not find /var/log/asterisk/mmlog"
	fi
	paste_post "/var/log/asterisk/mmlog"
	ls /var/log/asterisk/mmlog
elif [ "$cmd" = "backtrace-only" ]; then
	get_backtrace 0
elif [ "$cmd" = "backtrace" ]; then
	get_backtrace 1
elif [ "$cmd" = "rundump" ]; then
	coredump_prep
	nice -5 $AST_VARLIB_DIR/scripts/ast_coredumper --RUNNING > /tmp/ast_coredumper.txt
	corefullpath=$( grep "full.txt" /tmp/ast_coredumper.txt | cut -d' ' -f2 ) # the file is no longer necessarily simply just /tmp/core-full.txt
	if [ ! -f /tmp/ast_coredumper.txt ] || [ ${#corefullpath} -le 1 ]; then
		echoerr "Failed to get core dump of running process"
		printf "%s\n" "Make sure to start asterisk with -g or set dumpcore=yes in asterisk.conf"
		exit 2
	fi
	cat /tmp/ast_coredumper.txt
	rm -f /tmp/ast_coredumper.txt
	printf "%s\n" "Uploading paste of backtrace..."
	paste_post "$corefullpath"
elif [ "$cmd" = "threads" ]; then
	# Debug CPU usage of specific threads.
	pid=`cat /var/run/asterisk/asterisk.pid`
	if [ ${#pid} -eq 0 ]; then
		die "Asterisk is not currently running."
	fi
	ps -o pid,lwp,pcpu,pmem,comm,cmd -L $pid
	/sbin/asterisk -rx "core show threads"
	# Running phreaknet rundump here is probably useful as well.
elif [ "$cmd" = "reftrace" ]; then
	# Should be compiled with REF_DEBUG
	# Asterisk should be stopped (not running)
	/var/lib/asterisk/scripts/refcounter.py -f /var/log/asterisk/refs -n > /tmp/refs.txt
	ls -la /tmp/refs.txt
elif [ "$cmd" = "ccache" ]; then
	cd $AST_SOURCE_PARENT_DIR
	wget https://github.com/ccache/ccache/releases/download/v4.5.1/ccache-4.5.1.tar.gz
	if [ $? -ne 0 ]; then
		exit 1
	fi
	tar -zxvf ccache-4.5.1.tar.gz
	cd ccache-4.5.1
	cmake -DCMAKE_BUILD_TYPE=Release -DZSTD_FROM_INTERNET=ON -DHIREDIS_FROM_INTERNET=ON ..
	$AST_MAKE
	$AST_MAKE install
	if [ $? -ne 0 ]; then
		exit 1
	fi
	which ccache
	ln -s ccache /usr/local/bin/gcc
	ln -s ccache /usr/local/bin/g++
	ln -s ccache /usr/local/bin/cc
	ln -s ccache /usr/local/bin/c++
	which gcc
elif [ "$cmd" = "apiban" ]; then # install apiban-client: https://github.com/palner/apiban
	if [ ${#APIBAN_KEY} -le 1 ]; then
		read -r -p "apiban API key: " APIBAN_KEY
	fi
	if [ ! -d /usr/local/bin/apiban ]; then
		mkdir /usr/local/bin/apiban
	fi
	cd /usr/local/bin/apiban
	if [ ! -f apiban-iptables-client ]; then
		wget https://github.com/palner/apiban/raw/v0.7.0/clients/go/apiban-iptables-client
	fi
	if [ ! -f config.json ]; then
		wget https://raw.githubusercontent.com/palner/apiban/v0.7.0/clients/go/apiban-iptables/config.json
	fi
	if [ ${#APIBAN_KEY} -gt 0 ]; then
		sed -i "s/MY API KEY/$APIBAN_KEY/" config.json
	fi
	chmod +x /usr/local/bin/apiban/apiban-iptables-client
	cat > /etc/logrotate.d/apiban-client << EOF
/var/log/apiban-client.log {
        daily
        copytruncate
        rotate 7
        compress
}
EOF
	if [ ! -f /usr/local/bin/apiban/apiban-iptables-client ]; then
		die "apiban-client not installed successfully"
	fi
	/usr/local/bin/apiban/apiban-iptables-client
	printf "%s\n\n" "apiban-client has been installed"
	printf "%s\n" "To run this regularly, run crontab -e and add:"
	printf "%s\n" "*/4 * * * * /usr/local/bin/apiban/apiban-iptables-client >/dev/null 2>&1"
	# EDITOR=nano crontab -e
elif [ "$cmd" = "iaxping" ]; then
	if [ ${#2} -eq 0 ]; then
		die "Usage: phreaknet iaxping <hostname> [<port>]"
	fi
	port=4569
	host="$2"
	if [ ${#3} -gt 0 ]; then
		if [ "$3" -eq "$3" ] 2> /dev/null; then
			port="$3"
		else
			die "Invalid port number: $3"
		fi
	fi
	ensure_installed nmap
	exec nmap -sU ${host} -p ${port}
elif [ "$cmd" = "pcap" ]; then
	ensure_installed tshark
	debugtime=$EPOCHSECONDS
	if [ "$debugtime" = "" ]; then
		debugtime=`awk 'BEGIN {srand(); print srand()}'` # https://stackoverflow.com/a/41324810
	fi
	ip=""
	if [ ${#2} -gt 0 ]; then
		ip=$2
		printf "%s\n" "Starting packet capture for IP $ip - press CTRL+C to conclude"
		tshark -f "not port 22 and host $ip" -i any -w /tmp/pcap_$debugtime.pcap &
	else
		printf "%s\n" "Starting packet capture (all IPs) - press CTRL+C to conclude"
		tshark -f "not port 22" -i any -w /tmp/pcap_$debugtime.pcap &
	fi
	BGPID=$!
	trap handler INT
	wait $BGPID
	printf "Packet capture successfully saved to %s\n" "/tmp/pcap_$debugtime.pcap"
elif [ "$cmd" = "pcaps" ]; then
	ensure_installed tshark
	debugtime=$EPOCHSECONDS
	if [ "$debugtime" = "" ]; then
		debugtime=`awk 'BEGIN {srand(); print srand()}'` # https://stackoverflow.com/a/41324810
	fi
	ip=""
	if [ ${#2} -gt 0 ]; then
		ip=$2
		printf "%s\n" "Starting packet capture for IP $ip - press CTRL+C to conclude"
		tshark -f "not port 22 and host $ip" -i any -w /tmp/pcap_$debugtime.pcap &
	else
		printf "%s\n" "Starting packet capture (all IPs) - press CTRL+C to conclude"
		tshark -f "not port 22" -i any -w /tmp/pcap_$debugtime.pcap &
	fi
	BGPID=$!
	trap handler INT
	wait $BGPID
	printf "Packet capture successfully saved to %s\n" "/tmp/pcap_$debugtime.pcap"
	exec sngrep -I /tmp/pcap_$debugtime.pcap
elif [ "$cmd" = "sngrep" ]; then
	ensure_installed sngrep
	exec sngrep
elif [ "$cmd" = "freedisk" ]; then
	df -h /
	apt-get clean
	logrotate -f /etc/logrotate.conf
	rm -f /var/log/*.gz
	rm -f /var/log/asterisk/*.gz
	if [ -f /var/log/asterisk/full.1 ]; then
		rm /var/log/asterisk/full.1
	fi
	if [ -f /var/log/apache2/modsec_audit.log ]; then
		echo "" > /var/log/apache2/modsec_audit.log
	fi
	if [ -f /var/log/asterisk/security ]; then
		echo "" > /var/log/asterisk/security
	fi
	if [ -d /var/crash ]; then
		rm -f /var/crash/core*
	fi
	if [ -f /var/log/asterisk/refs ]; then
		echo "" > /var/log/asterisk/refs
	fi
	rm -f /tmp/core*
	rm -rf /var/lib/snapd/cache/* # can be safely removed: https://askubuntu.com/questions/1075050/how-to-remove-uninstalled-snaps-from-cache/1156686#1156686
	snap services
	snap remove hello-world
	set -eu
	snap list --all | awk '/disabled/{print $1, $3}' |
    while read snapname revision; do
        snap remove "$snapname" --revision="$revision"
    done
	service fail2ban stop
	rm -rf /var/lib/fail2ban
	service fail2ban start
	if [ "$PAC_MAN" = "apt-get" ]; then
		apt autoremove -y
	fi
	rm -f /tmp/core*
	df -h /
elif [ "$cmd" = "topdisk" ]; then
	df -h -x squashfs -x tmpfs -x devtmpfs
	find / -printf '%s %p\n'| sort -nr | head -50
elif [ "$cmd" = "topdir" ]; then
	# For largest directories in current directory:
	du -sh * | sort -rh
elif [ "$cmd" = "enable-swap" ]; then
# https://www.digitalocean.com/community/tutorials/how-to-add-swap-space-on-debian-10
	assert_root
	swap=`swapon --show | wc -l | tr -d '\n'`
	if [ "$swap" = "0" ]; then
		echo "Swap is currently disabled"
	else
		echoerr "Swap is already enabled"
		swapon --show
		exit 1
	fi
	freediskspace=`df / | awk '{print $4}' | tail -n +2 | tr -d '\n'`
	if [ $freediskspace -gt 1300000 ]; then
		echo "More than 1.3 GB of free disk space, allocating 1 GB swap"
		fallocate -l 1G /swapfile
	elif [ $freediskspace -gt 800000 ]; then
		echo "More than 800 MB of free disk space, allocating 500 MB space"
		fallocate -l 500M /swapfile
	else
		die "Only $freediskspace KB of space available (not enough to add sufficient swap)"
	fi
	chmod 600 /swapfile
	mkswap /swapfile
	swapon /swapfile
	swapon --show
	free -h
	# To make the swap permanent:
	# cp /etc/fstab /etc/fstab.bak
	# echo '/swapfile none swap sw 0 0' | tee -a /etc/fstab
	cat /proc/sys/vm/swappiness
	sysctl vm.swappiness=60 # should be high but not too high, see: https://stackoverflow.com/questions/44954336/gdb-commits-suicide-kills-debugged-process-out-of-a-sudden
	sysctl vm.vfs_cache_pressure=50
elif [ "$cmd" = "disable-swap" ]; then
	assert_root
	swap=`swapon --show | wc -l | tr -d '\n'`
	if [ "$swap" != "0" ]; then
		swapoff -a
		echo "Swap is currently enabled, disabling"
	else
		echo "Swap is already disabled"
	fi
	swapon --show
	# if in /etc/fstab, then should be removed from there, too!
	if [ ! -f /swapfile ]; then
		die "/swapfile does not exist!"
	fi
	df -h /
	rm /swapfile
	df -h /
	echo "Removed swapfile!"
elif [ "$cmd" = "examples" ]; then
	printf "%s\n\n" 	"========= PhreakScript Example Usages ========="
	printf "%s\n\n" 	"Presented in the logical order of usage, but with multiple variations for each command."
	printf "%s\n\n" 	"Installation commands:"
	printf "%s\n"		"phreaknet install                  Install the latest version of Asterisk."
	printf "%s\n"		"phreaknet install --cc=44          Install the latest version of Asterisk, with country code 44."
	printf "%s\n"		"phreaknet install --force          Reinstall the latest version of Asterisk."
	printf "%s\n"		"phreaknet install --dahdi          Install the latest version of Asterisk, with DAHDI."
	printf "%s\n"		"phreaknet install --sip --weaktls  Install Asterisk with chan_sip built AND support for TLS 1.0."
	printf "%s\n"		"phreaknet install --version 18.9.0 Install Asterisk version 18.9.0 as the base version of Asterisk."
	printf "%s\n"		"phreaknet install --version=20     Install latest version of Asterisk 20 as the base version of Asterisk."
	printf "%s\n"		"phreaknet installts                Install Asterisk Test Suite and Unit Test support (developers only)."
	printf "%s\n"		"phreaknet pulsar                   Install revertive pulsing pulsar AGI, with bug fixes"
	printf "%s\n"		"phreaknet sounds --boilerplate     Install Pat Fleet sounds and basic boilerplate old city tone audio."
	printf "%s\n"		"phreaknet config --force --api-key=<KEY> --clli=<CLLI> --disa=<DISA>"
	printf "%s\n"		"                                   Download and initialize boilerplate PhreakNet configuration"
	printf "%s\n"		"phreaknet keygen                   Upload existing RSA public key to PhreakNet"
	printf "%s\n"		"phreaknet keygen --rotate          Create or rotate PhreakNet RSA keypair, then upload public key to PhreakNet"
	printf "%s\n"		"phreaknet validate                 Validate your dialplan configuration and check for errors"
	printf "\n%s\n\n"	"Debugging commands:"
	printf "%s\n"		"phreaknet trace                    Perform a trace with verbosity 10 and no debug level (notify Bus. Ofc.)"
	printf "%s\n"		"phreaknet trace --debug 1          Perform a trace with verbosity 10 and debug level 1 (notify Bus. Ofc.)"
	printf "%s\n"		"phreaknet backtrace      		    Process, extract, and upload a core dump"
	printf "\n%s\n\n"	"Maintenance commands:"
	printf "%s\n"		"phreaknet update                   Update PhreakScript. No Asterisk or configuration modification will occur."
	printf "%s\n"		"phreaknet update --upstream=URL    Update PhreakScript using URL as the upstream source (for testing)."
	printf "%s\n"		"phreaknet patch                    Apply the latest PhreakNet configuration patches."
	printf "%s\n"		"phreaknet fullpatch app_verify     Download the latest version of the app_verify module."
	printf "\n"
elif [ "$cmd" = "version" ]; then
	printf "%s" "PhreakScript "
	grep "# v" $FILE_PATH | head -1 | cut -d'v' -f2
elif [ "$cmd" = "info" ]; then
	phreakscript_info
elif [ "$cmd" = "about" ]; then
	printf "%s\n%s\n%s\n\n" "========= PhreakScript =========" "PhreakScript automates the management of Asterisk and DAHDI, from installation to patching to debugging." "The version of Asterisk and DAHDI installed by PhreakScript isn't a fork of Asterisk/DAHDI. Rather, it builds on top of the latest versions of Asterisk and DAHDI, so that users benefit from bug fixes and new features and improvements upstream, but also adds additional bug fixes and features that haven't made it upstream, to provide the fullest and richest Asterisk/DAHDI experience. For more details, see https://phreaknet.org/changes" | fold -s -w 80
	printf "%s\n" "Change Log (both changes to PhreakScript and to Asterisk/DAHDI as installed by PhreakScript)"
	grep "^#" $FILE_PATH | grep -A 999999 "Begin Change Log" | grep -B 999999 "End Change Log" | tail -n +2 | head -n -1 | cut -c 3-
	printf "\n"
	phreakscript_info
else
	echoerr "Invalid command: $cmd, type phreaknet help for usage."
	exit 2
fi

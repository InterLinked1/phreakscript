#!/bin/sh

# PhreakScript
# (C) 2021-2022 PhreakNet - https://portal.phreaknet.org and https://docs.phreaknet.org
# v0.1.32 (2021-01-19)

# Setup (as root):
# cd /usr/local/src
# wget https://docs.phreaknet.org/script/phreaknet.sh
# chmod +x phreaknet.sh
# ./phreaknet.sh make
# phreaknet update
# phreaknet install

## Begin Change Log:
# 2021-01-19 0.1.32 Asterisk: add app_playdigits
# 2021-01-18 0.1.31 Asterisk: Temper SRTCP warnings
# 2021-01-10 0.1.30 Asterisk: add res_coindetect
# 2021-01-08 0.1.29 Asterisk: add app_randomplayback
# 2021-01-07 0.1.28 Asterisk: add app_pulsar
# 2021-01-04 0.1.27 Asterisk: add app_saytelnumber
# 2022-01-01 0.1.26 PhreakScript: removed hardcoded paths
# 2021-12-31 0.1.25 PhreakScript: added ulaw command, Asterisk: added func_frameintercept, app_fsk
# 2021-12-27 0.1.24 PhreakScript: add tests for func_dbchan
# 2021-12-26 0.1.23 PhreakScript: added Asterisk build info to trace, Asterisk: added app_loopplayback, func_numpeer
# 2021-12-24 0.1.22 PhreakScript: fix path issues, remove upgrade command
# 2021-12-20 0.1.21 Asterisk: update target usecallmanager
# 2021-12-17 0.1.20 PhreakScript: add tests for verify, added backtrace enable
# 2021-12-16 0.1.19 PhreakScript: added support for building chan_sip with Cisco Call Manager phone support
# 2021-12-15 0.1.18 PhreakScript: added runtests, Asterisk: update func_evalexten
# 2021-12-14 0.1.17: Asterisk: update func_evalexten, PhreakScript: added gerrit command
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
AST_SOURCE_NAME="asterisk-18-current"
CISCO_CM_SIP="cisco-usecallmanager-18.7.0"
AST_ALT_VER=""
MIN_ARGS=1
FILE_DIR="$( cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
FILE_NAME=$( basename $0 ) # grr... why is realpath not in the POSIX standard?
FILE_PATH="$FILE_DIR/$FILE_NAME"
PATCH_DIR=https://docs.phreaknet.org/script
OS_DIST_INFO="(lsb_release -ds || cat /etc/*release || uname -om ) 2>/dev/null | head -n1 | cut -d'=' -f2"
OS_DIST_INFO=$(eval "$OS_DIST_INFO" | tr -d '"')
PAC_MAN="apt-get"
AST_SOUNDS_DIR="$AST_VARLIB_DIR/sounds/en"
AST_MOH_DIR="$AST_VARLIB_DIR/moh"
AST_MAKE="make"
PATH="/sbin:$PATH" # in case su used without path

# Defaults
AST_CC=1 # Country Code (default: 1 - NANPA)
AST_USER=""
WEAK_TLS=0
CHAN_SIP=0
SIP_CISCO=0
CHAN_SCCP=0
CHAN_DAHDI=0
TEST_SUITE=0
FORCE_INSTALL=0
ENABLE_BACKTRACES=0
ASTKEYGEN=0
DEFAULT_PATCH_DIR="/var/www/html/interlinked/sites/phreak/code/asterisk/patches" # for new patches
PHREAKNET_CLLI=""
INTERLINKED_APIKEY=""
BOILERPLATE_SOUNDS=0
SCRIPT_UPSTREAM="$PATCH_DIR/phreaknet.sh"
DEBUG_LEVEL=0

echog() { printf "\e[32;1m%s\e[0m\n" "$*" >&2; }
echoerr() { printf "\e[31;1m%s\e[0m\n" "$*" >&2; } # https://stackoverflow.com/questions/2990414/echo-that-outputs-to-stderr

if [ "$OS_DIST_INFO" = "FreeBSD" ]; then
	PAC_MAN="pkg"
	AST_SOURCE_PARENT_DIR="/usr/local/src"
	AST_MAKE="gmake"
fi

phreakscript_info() {
	printf "%s" "Hostname: "
	hostname
	echo $OS_DIST_INFO
	echo "Package Manager: $PAC_MAN"
	asterisk -V 2> /dev/null # Asterisk might or might not exist...
	if [ -d /etc/dahdi ]; then
		dahdi_cfg -tv 2>&1 | grep "ersion"
	fi
	printf "%s" "PhreakScript "
	grep "# v" $FILE_PATH | head -1 | cut -d'v' -f2
	echo "https://github.com/InterLinked1/phreakscript"
	echo "(C) 2021 PhreakNet - https://portal.phreaknet.org https://docs.phreaknet.org"
	echo "To report bugs or request feature additions, please report at https://issues.interlinked.us (also see https://docs.phreaknet.org/#contributions) and/or post to the PhreakNet mailing list: https://groups.io/g/phreaknet" | fold -s -w 120
}

usage() {
	#complete -W "make help install installts pulsar sounds config keygen edit genpatch patch update upgrade trace backtrace" phreaknet # has to be manually entered, at present
	#source ~/.bash_profile
	echo "Usage: phreaknet [command] [options]
Commands:
   *** Getting Started ***
   about             About PhreakScript
   help              Program usage
   examples          Example usages
   info              System info

   *** First Use / Installation ***
   make              Add PhreakScript to path
   install           Install or upgrade PhreakNet Asterisk
   installts         Install Asterisk Test Suite
   pulsar            Install Revertive Pulsing simulator
   sounds            Install Pat Fleet sound library
   ulaw              Convert wav to ulaw (specific file or all in current directory)

   *** Initial Configuration ***
   config            Install PhreakNet boilerplate config
   keygen            Install and update PhreakNet RSA keys

   *** Maintenance ***
   update            Update PhreakScript
   patch             Patch PhreakNet Asterisk configuration
   genpatch          Generate a PhreakPatch

   *** Debugging ***
   validate          Run dialplan validation and diagnostics and look for problems
   trace             Capture a CLI trace and upload to InterLinked Paste
   pcap              Perform a packet capture, optionally against a specific IP address
   enable-backtraces Enables backtraces to be extracted from the core dumper (new or existing installs)
   backtrace         Use astcoredumper to process a backtrace and upload to InterLinked Paste
   backtrace-only    Use astcoredumper to process a backtrace
   valgrind          Run Asterisk under valgrind

   *** Development & Testing ***
   docverify         Show documentation validation errors and details
   runtests          Run differential PhreakNet tests
   runtest           Run any specified test (argument to command)
   stresstest        Run any specified test multiple times in a row
   gerrit            Manually install a custom patch set from Gerrit
   ccache            Globally install ccache to speed up recompilation

   *** Miscellaneous ***
   docgen            Generate Asterisk user documentation
   pubdocs           Generate Asterisk user documentation (deprecated)
   edit              Edit PhreakScript

Options:
   -b, --backtraces   Enables getting backtraces
   -c, --cc           Country Code (default is 1)
   -d, --dahdi        Install DAHDI
   -f, --force        Force install or config
   -h, --help         Display usage
   -o, --flag-test    Option flag test
   -s, --sip          Install chan_sip instead of or in addition to chan_pjsip
   -t, --testsuite    Compile with developer support for Asterisk test suite and unit tests
   -u, --user         User as which to run Asterisk (non-root)
   -v, --version      Specific version of Asterisk to install (M.m.b e.g. 18.8.0)
   -w, --weaktls      Allow less secure TLS versions down to TLS 1.0 (default is 1.2+)
       --cisco        Add full support for Cisco Call Manager phones (chan_sip only)
       --sccp         Install chan_sccp channel driver (Cisco Skinny)
       --boilerplate  sounds: Also install boilerplate sounds
       --clli         config: CLLI code
       --debug        trace: Debug level (default is 0/OFF, max is 10)
       --disa         config: DISA number
       --api-key      config: InterLinked API key
       --rotate       keygen: Rotate/create keys
       --upstream     update: Specify upstream source
"
	phreakscript_info
	exit 2
}

install_prereq() {
	if [ "$PAC_MAN" = "apt-get" ]; then
		apt-get clean
		apt-get update -y
		apt-get upgrade -y
		apt-get dist-upgrade -y
		apt-get install -y ntp iptables tcpdump wget curl sox libcurl4-openssl-dev mpg123 dnsutils php festival bc fail2ban mariadb-server git
		# used to feed the country code non-interactively
		apt-get install libcurl3-gnutls=7.64.0-4+deb10u2 # fix git clone not working: upvoted comment at https://superuser.com/a/1642989
		apt-get install -y debconf-utils
		apt-get -y autoremove
	elif [ "$PAC_MAN" = "pkg" ]; then
		pkg update -y
		pkg upgrade -y
		pkg install -y e2fsprogs-libuuid wget sqlite3 ntp tcpdump curl sox mpg123 git bind-tools gmake subversion # bind-tools for dig
	else
		echoerr "Could not determine what package manager to use..."
	fi
}

run_testsuite_test() {
	testcount=$(($testcount + 1))
	./runtests.py --test=tests/$1
	if [ $? -ne 0 ]; then # test failed
		lastrun=`ls -d -v logs/$1/* | tail -1` # get the directory containing the logs from the most recently run test
		ls "$lastrun/ast1/var/log/asterisk/full.txt"
		grep -B 12 "UserEvent(" $lastrun/ast1/var/log/asterisk/full.txt | grep -v "pbx.c: Launching" | grep -v "stasis.c: Topic" # this should provide a good idea of what failed (or at least, what didn't succeed)
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
		$AST_SOURCE_PARENT_DIR/testsuite/runtests.py --test=tests/$1
		if [ $? -ne 0 ]; then # test failed
			lastrun=`ls -d -v $AST_SOURCE_PARENT_DIR/testsuite/logs/$1/* | tail -1` # get the directory containing the logs from the most recently run test
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
	cd $AST_SOURCE_PARENT_DIR/testsuite

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
	cp -r "phreakscript/testsuite/tests/$1" "testsuite/tests/$parent"
	echo "    - test: '$base'" >> "testsuite/tests/$parent/tests.yaml" # rather than have a patch for this file, which could be subject to frequent change and result in a finicky patch that's likely to fail, simply append it to the list of tests to run
	printf "%s\n" "Installed test: %s" "$1"
}

add_phreak_testsuite() {
	printf "%s\n" "Applying PhreakNet test suite additions"
	cd ..
	git clone https://github.com/InterLinked1/phreakscript.git
	cd phreakscript
	git config pull.rebase false # this is the default. do this solely to avoid those annoying "Pulling without specifying" warnings...
	git pull # in case it already existed, update the repo
	cd ..

	install_phreak_testsuite_test "apps/dialtone"
	install_phreak_testsuite_test "apps/verify"
	install_phreak_testsuite_test "funcs/func_dbchan"

	printf "%s\n" "Finished patching testsuite"
}

install_testsuite_itself() {
	apt-get clean
	apt-get update -y
	apt-get upgrade -y
	apt-get install -y liblua5.1-0-dev lua5.3 git python python-setuptools

	# Python 2 support is going away in Debian
	curl https://bootstrap.pypa.io/pip/2.7/get-pip.py -o get-pip.py # https://stackoverflow.com/a/64240018/6110631
	python get-pip.py
	pip2 install pyyaml
	pip2 install twisted

	cd $AST_SOURCE_PARENT_DIR
	if [ -d "testsuite" ]; then
		if [ "$FORCE_INSTALL" = "1" ]; then
			printf "%s\n" "Reinstalling testsuite..."
			rm -rf testsuite
		else
			echoerr "Test Suite already exists... specify --force flag to reinstall"
		fi
	fi
	git clone https://gerrit.asterisk.org/testsuite
	if [ $? -ne 0 ]; then
		echoerr "Failed to download testsuite..."
		return 1
	fi
	cd testsuite/asttest
	make
	make install
	asttest
	cd ..
	cd addons
	make update
	cd starpy
	python setup.py install
	cd ../..
	apt-get install -y python-twisted
	pip install twisted
	# sipp
	cd $AST_SOURCE_PARENT_DIR
	git clone https://github.com/SIPp/sipp.git
	cd sipp
	if [ "$PAC_MAN" = "apt-get" ]; then
		apt-get install -y cmake libsctp-dev libgsl-dev
	fi
	git checkout v3.6.0 ## This is the latest version we KNOW works.
	./build.sh --prefix=/usr --with-openssl --with-pcap --with-rtpstream --with-sctp --with-gsl CFLAGS=-w
	./sipp -v
	make install
	cd ..
	cd testsuite
	# ./runtests.py -t tests/channels/iax2/basic-call/ # run a single basic test
	./runtests.py -l
	add_phreak_testsuite
	printf "%s\n" "Asterisk Test Suite installation complete"
}

configure_devmode() {
	./configure --enable-dev-mode
	make menuselect.makeopts
	menuselect/menuselect --enable DONT_OPTIMIZE --enable BETTER_BACKTRACES --enable TEST_FRAMEWORK --enable DO_CRASH menuselect.makeopts
	menuselect/menuselect --enable-category MENUSELECT_TESTS menuselect.makeopts
}

install_testsuite() { # $1 = $FORCE_INSTALL
	cd $AST_SOURCE_PARENT_DIR
	# in case we're not already in the right directory
	AST_SRC_DIR=`ls -d */ | grep "^asterisk-" | tail -1`
	if [ "$AST_SRC_DIR" = "" ]; then
		echoerr "Asterisk source not found. Aborting..."
		return 1
	fi
	cd $AST_SOURCE_PARENT_DIR/$AST_SRC_DIR
	configure_devmode
	make
	make install
	install_testsuite_itself
}

gerrit_patch() {
	printf "%s\n" "Downloading and applying Gerrit patch $1"
	wget -q $2 -O $1.diff.base64
	if [ $? -ne 0 ]; then
		echoerr "Patch download failed"
		exit 2
	fi
	if [ "$OS_DIST_INFO" = "FreeBSD" ]; then
		b64decode -r $1.diff.base64 > $1.diff
		if [ $? -ne 0 ]; then
			exit 2
		fi
	else
		base64 --decode $1.diff.base64 > $1.diff
	fi
	git apply $1.diff
	if [ $? -ne 0 ]; then
		echoerr "Failed to apply Gerrit patch... this should be reported..."
		if [ "$FORCE_INSTALL" = "1" ]; then
			sleep 2
		else
			exit 2
		fi
	fi
	rm $1.diff.base64 $1.diff
}

dahdi_undo() {
	printf "Restoring drivers by undoing PATCH: %s\n" "$3"
	wget -q "https://git.asterisk.org/gitweb/?p=dahdi/linux.git;a=patch;h=$4" -O /tmp/$2.patch --no-cache
	patch -u -b -p 1 --reverse -i /tmp/$2.patch
	if [ $? -ne 0 ]; then
		echoerr "Failed to reverse DAHDI patch... this should be reported..."
		exit 2
	fi
	rm /tmp/$2.patch
}

dahdi_custom_patch() {
	printf "Applying custom DAHDI patch: %s\n" "$1"
	wget -q "$3" -O /tmp/$1.patch --no-cache
	patch -u -b "$AST_SOURCE_PARENT_DIR/$2" -i /tmp/$1.patch
	if [ $? -ne 0 ]; then
		echoerr "Failed to apply DAHDI patch... this should be reported..."
		exit 2
	fi
	rm /tmp/$1.patch
}

dahdi_patch() {
	printf "Applying unmerged DAHDI patch: %s\n" "$1"
	wget -q "https://git.asterisk.org/gitweb/?p=dahdi/linux.git;a=patch;h=$1" -O /tmp/$1.patch --no-cache
	patch -u -b -p 1 -i /tmp/$1.patch
	if [ $? -ne 0 ]; then
		echoerr "Failed to apply DAHDI patch... this should be reported..."
		exit 2
	fi
	rm /tmp/$1.patch
}

dahdi_unpurge() { # undo "great purge" of 2018: $1 = DAHDI_LIN_SRC_DIR
	printf "%s\n" "Reverting patches that removed driver support in DAHDI..."
	dahdi_undo $1 "dahdi_pci_module" "Remove unnecessary dahdi_pci_module macro" "4af6f69fff19ea3cfb9ab0ff86a681be86045215"
	dahdi_undo $1 "dahdi_irq_handler" "Remove unnecessary DAHDI_IRQ_HANDLER macro" "cdd6ddd0fd08cb8b7313b16074882439fbb58045"
	dahdi_undo $1 "devtype" "Remove struct devtype for unsupported drivers" "75620dd9ef6ac746745a1ecab4ef925a5b9e2988"
	dahdi_undo $1 "wcb" "Remove support for all but wcb41xp wcb43xp and wcb23xp." "29cb229cd3f1d252872b7f1924b6e3be941f7ad3"
	dahdi_undo $1 "wctdm" "Remove support for wctdm800, wcaex800, wctdm410, wcaex410." "a66e88e666229092a96d54e5873d4b3ae79b1ce3"
	dahdi_undo $1 "wcte12xp" "Remove support for wcte12xp." "3697450317a7bd60bfa7031aad250085928d5c47"
	dahdi_custom_patch "wcte12xp_base" "$1/drivers/dahdi/wcte12xp/base.c" "https://raw.githubusercontent.com/InterLinked1/phreakscript/master/patches/wcte12xp_base.diff" # bug fix for case statement fallthrough
	dahdi_undo $1 "wcte11xp" "Remove support for wcte11xp." "3748456d22122cf807b47d5cf6e2ff23183f440d"
	dahdi_undo $1 "wctdm" "Remove support for wctdm." "04e759f9c5a6f76ed88bc6ba6446fb0c23c1ff55"
	dahdi_undo $1 "wct1xxp" "Remove support for wct1xxp." "dade6ac6154b58c4f5b6f178cc09de397359000b"
	dahdi_undo $1 "wcfxo" "Remove support for wcfxo." "14198aee8532bbafed2ad1297177f8e0e0f13f50"
	dahdi_undo $1 "tor2" "Remove support for tor2." "60d058cc7a064b6e07889f76dd9514059c303e0f"
	dahdi_undo $1 "pciradio" "Remove support for pciradio." "bfdfc4728c033381656b59bf83aa37187b5dfca8"
	printf "%s\n" "Finished undoing DAHDI removals!"
}

phreak_tree_module() { # $1 = file to patch
	printf "Adding new module: %s\n" "$1"
	wget -q "https://raw.githubusercontent.com/InterLinked1/phreakscript/master/$1" -O "$AST_SOURCE_PARENT_DIR/$AST_SRC_DIR/$1" --no-cache
	if [ $? -ne 0 ]; then
		echoerr "Failed to download module: $2"
		exit 2
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
	rm "/tmp/$2"
}

phreak_tree_patch() { # $1 = patched file, $2 = patch name
	printf "Applying patch %s to %s\n" "$2" "$1"
	wget -q "https://raw.githubusercontent.com/InterLinked1/phreakscript/master/patches/$2" -O "/tmp/$2" --no-cache
	if [ $? -ne 0 ]; then
		echoerr "Failed to download patch: $2"
		exit 2
	fi
	patch -u -b "$1" -i "/tmp/$2"
	rm "/tmp/$2"
}

phreak_patches() { # $1 = $PATCH_DIR, $2 = $AST_SRC_DIR
	### Inject custom PhreakNet patches to add additional functionality and features.
	###  If/when/as these are integrated upstream, they will be removed from this function. 

	cd $AST_SOURCE_PARENT_DIR/$2

	## Add Standalone PhreakNet Modules
	phreak_tree_module "apps/app_dialtone.c"
	phreak_tree_module "apps/app_frame.c"
	phreak_tree_module "apps/app_loopplayback.c"
	phreak_tree_module "apps/app_mail.c"
	phreak_tree_module "apps/app_memory.c"
	phreak_tree_module "apps/app_playdigits.c"
	phreak_tree_module "apps/app_pulsar.c"
	phreak_tree_module "apps/app_randomplayback.c"
	phreak_tree_module "apps/app_saytelnumber.c"
	phreak_tree_module "apps/app_softmodem.c"
	phreak_tree_module "apps/app_streamsilence.c"
	phreak_tree_module "apps/app_tonetest.c"
	phreak_tree_module "apps/app_verify.c"
	phreak_tree_module "apps/app_keyprefetch.c"
	phreak_tree_module "configs/samples/verify.conf.sample"
	phreak_tree_module "funcs/func_dbchan.c"
	phreak_tree_module "funcs/func_notchfilter.c"
	phreak_tree_module "funcs/func_numpeer.c"
	phreak_tree_module "funcs/func_ochannel.c"
	phreak_tree_module "res/res_coindetect.c"

	## Third Party Modules
	printf "Adding new module: %s\n" "apps/app_tdd.c"
	wget -q https://raw.githubusercontent.com/dgorski/app_tdd/main/app_tdd.c -O $AST_SOURCE_PARENT_DIR/$2/apps/app_tdd.c --no-cache
	wget -q https://raw.githubusercontent.com/alessandrocarminati/app-fsk/master/app_fsk_18.c -O $AST_SOURCE_PARENT_DIR/$2/apps/app_fsk.c --no-cache
	sed -i 's/<defaultenabled>no<\/defaultenabled>//g' apps/app_fsk.c # temporary bug fix

	## Add patches to existing modules
	phreak_nontree_patch "main/translate.c" "translate.diff" "https://issues.asterisk.org/jira/secure/attachment/60464/translate.diff" # Bug fix to translation code
	phreak_tree_patch "apps/app_stack.c" "returnif.patch" # Add ReturnIf application
	phreak_tree_patch "apps/app_dial.c" "6112308.diff" # Bug fix to app_dial (prevent infinite loop)
	phreak_tree_patch "channels/chan_sip.c" "sipfaxcontrol.diff" # Add fax timing controls to SIP channel driver
	phreak_tree_patch "main/loader.c" "loader_deprecated.patch" # Don't throw alarmist warnings for deprecated ADSI modules that aren't being removed
	phreak_tree_patch "main/manager_channels.c" "disablenewexten.diff" # Disable Newexten event, which significantly degrades dialplan performance
	phreak_tree_patch "main/dsp.c" "coindsp.patch" # DSP additions

	if [ "$WEAK_TLS" = "1" ]; then
		phreak_tree_patch "res/res_srtp.c" "srtp.diff" # Temper SRTCP unprotect warnings. Only required for older ATAs that require older TLS protocols.
	fi

	## Gerrit patches: merged, remove in 18.10
	gerrit_patch 16634 "https://gerrit.asterisk.org/changes/asterisk~16634/revisions/16/patch?download" # func_json
	gerrit_patch 16499 "https://gerrit.asterisk.org/changes/asterisk~16499/revisions/3/patch?download" # app_mf: Add ReceiveMF
	gerrit_patch 17510 "https://gerrit.asterisk.org/changes/asterisk~17510/revisions/1/patch?download" # app_sendtext: Add ReceiveText
	gerrit_patch 17470 "https://gerrit.asterisk.org/changes/asterisk~17470/revisions/9/patch?download" # variable substitution for extensions
	gerrit_patch 17654 "https://gerrit.asterisk.org/changes/asterisk~17654/revisions/1/patch?download" # critical compiler fix
	gerrit_patch 17648 "https://gerrit.asterisk.org/changes/asterisk~17648/revisions/1/patch?download" # app.c: Throw warnings for nonexistent app options
	gerrit_patch 17652 "https://gerrit.asterisk.org/changes/asterisk~17652/revisions/7/patch?download" # app_sf

	## Gerrit patches: remove once merged
	phreak_tree_module "apps/app_if.c"
	gerrit_patch 16629 "https://gerrit.asterisk.org/changes/asterisk~16629/revisions/2/patch?download" # app_assert
	gerrit_patch 16075 "https://gerrit.asterisk.org/changes/asterisk~16075/revisions/21/patch?download" # func_evalexten
	gerrit_patch 17700 "https://gerrit.asterisk.org/changes/asterisk~17700/revisions/3/patch?download" # CLI command to unload/load module
	gerrit_patch 17714 "https://gerrit.asterisk.org/changes/asterisk~17714/revisions/3/patch?download" # CLI command to eval dialplan functions
	gerrit_patch 17716 "https://gerrit.asterisk.org/changes/asterisk~17716/revisions/3/patch?download" # func_frameintercept
	gerrit_patch 17719 "https://gerrit.asterisk.org/changes/asterisk~17719/revisions/7/patch?download" # res_pbx_validate
	gerrit_patch 17786 "https://gerrit.asterisk.org/changes/asterisk~17786/revisions/1/patch?download" # app_signal

	# Gerrit patches: never going to be merged upstream (do not remove):
	gerrit_patch 16569 "https://gerrit.asterisk.org/changes/asterisk~16569/revisions/5/patch?download" # chan_sip: Add custom parameters

	## Menuselect updates
	make menuselect.makeopts
	menuselect/menuselect --enable app_memory menuselect.makeopts # app_memory doesn't compile by default
}

phreak_gerrit_off() {
	gerritid=`echo "$1" | cut -d'~' -f2 | cut -d'/' -f1`
	if [ "${#gerritid}" -ne 5 ]; then
		echoerr "Invalid Gerrit review (id $gerritid)"
		exit 2
	fi
	gerrit_patch "$gerritid" "$1"
}

freebsd_port_patch() {
	filename=$( basename $1 )
	wget -q --show-progress "$1" -O "$filename" --no-cache
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
	freebsd_port_patch "https://raw.githubusercontent.com/freebsd/freebsd-ports/7abe6caf38ac7e552642372be9b4136c4354d0fd/net/asterisk18/files/patch-channels_sip_include_sip.h"
	freebsd_port_patch "https://raw.githubusercontent.com/freebsd/freebsd-ports/7abe6caf38ac7e552642372be9b4136c4354d0fd/net/asterisk18/files/patch-configure"
	freebsd_port_patch "https://raw.githubusercontent.com/freebsd/freebsd-ports/7abe6caf38ac7e552642372be9b4136c4354d0fd/net/asterisk18/files/patch-contrib_Makefile"
	freebsd_port_patch "https://raw.githubusercontent.com/freebsd/freebsd-ports/7abe6caf38ac7e552642372be9b4136c4354d0fd/net/asterisk18/files/patch-main_Makefile"
	freebsd_port_patch "https://raw.githubusercontent.com/freebsd/freebsd-ports/7abe6caf38ac7e552642372be9b4136c4354d0fd/net/asterisk18/files/patch-main_asterisk.exports.in"
	freebsd_port_patch "https://raw.githubusercontent.com/freebsd/freebsd-ports/7abe6caf38ac7e552642372be9b4136c4354d0fd/net/asterisk18/files/patch-main_http.c"
	freebsd_port_patch "https://raw.githubusercontent.com/freebsd/freebsd-ports/7abe6caf38ac7e552642372be9b4136c4354d0fd/net/asterisk18/files/patch-main_lock.c"
	freebsd_port_patch "https://raw.githubusercontent.com/freebsd/freebsd-ports/7abe6caf38ac7e552642372be9b4136c4354d0fd/net/asterisk18/files/patch-third-party_pjproject_Makefile"
	freebsd_port_patch "https://raw.githubusercontent.com/freebsd/freebsd-ports/7abe6caf38ac7e552642372be9b4136c4354d0fd/net/asterisk18/files/patch-third-party_pjproject_Makefile.rules"
	printf "%s\n" "FreeBSD patching complete..."
}

download_if_missing() {
	if [ ! -f "$1" ]; then
		wget "$2"
	else
		printf "Audio file already present, not overwriting: %s\n" "$1"
	fi
}

paste_post() { # $1 = file to upload
	if [ ${#INTERLINKED_APIKEY} -eq 0 ]; then
		INTERLINKED_APIKEY=`grep -R "interlinkedkey=" $AST_CONFIG_DIR | grep -v "your-interlinked-api-key" | cut -d'=' -f2 | awk '{print $1;}'`
		if [ ${#INTERLINKED_APIKEY} -lt 30 ]; then
			echoerr "Failed to find InterLinked API key. For future use, please set your [global] variables, e.g. by running phreaknet config"
			printf '\a' # alert the user
			read -r -p "InterLinked API key: " INTERLINKED_APIKEY
			if [ ${#INTERLINKED_APIKEY} -lt 30 ]; then
				echoerr "InterLinked API key too short. Not going to attempt uploading paste..."
				ls $1
				exit 2
			fi
		fi
	fi
	url=`curl -X POST -F "body=@$1" -F "key=$INTERLINKED_APIKEY" https://paste.interlinked.us/api/`
	printf "Paste URL: %s\n" "${url}.txt"
}

get_backtrace() { # $1 = "1" to upload
	if [ "$PAC_MAN" = "apt-get" ]; then
		apt-get install gdb # for astcoredumper
	fi
	$AST_VARLIB_DIR/scripts/ast_coredumper > /tmp/ast_coredumper.txt
	corefullpath=$( grep "full.txt" /tmp/ast_coredumper.txt | cut -d' ' -f2 ) # the file is no longer simply just /tmp/core-full.txt
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
	if [ "$1" == "1" ]; then # backtrace
		paste_post "$corefullpath"
		rm /tmp/core-*.txt
	else # backtrace-only
		ls $corefullpath
	fi
}

rule_audio() {
	cd $AST_SOUNDS_DIR # so we can use both relative and absolute paths
	# trying to use {} to combine the output of multiple commands causes issues, so do them serially
	directories="$AST_CONFIG_DIR/"
	pcregrep -rho1 --include='.*\.conf$' ',Playback\((.*)?\)' $directories | grep -vx ';.*' | cut -d',' -f1 > /tmp/phreakscript_update.txt
	pcregrep -rho1 --include='.*\.conf$' ',BackGround\((.*)?\)' $directories | grep -vx ';.*' | cut -d',' -f1 >> /tmp/phreakscript_update.txt
	pcregrep -rho1 --include='.*\.conf$' ',Read\((.*)?\)' $directories | grep -vx ';.*' | cut -s -d',' -f2 | grep -v "dial" | grep -v "stutter" | grep -v "congestion" >> /tmp/phreakscript_update.txt
	filenames=$(cat /tmp/phreakscript_update.txt | cut -d')' -f1 | tr '&' '\n' | sed '/^$/d' | sort | uniq | grep -v "\${" | xargs  -d "\n" -n1 -I{} sh -c 'if ! test -f "{}.ulaw"; then echo "{}"; fi')
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
	results=$(pcregrep -ro3 --include='.*\.conf$' -hi '([1n\)]),(Goto|Gosub|GotoIf|GosubIf)\(([A-Za-z0-9_-]*),([A-Za-z0-9:\\$\{\}_-]*),([A-Za-z0-9:\\$\{\}_-]*)([\)\(:])' $AST_CONFIG_DIR/ | sort | uniq | xargs -n1 -I{} sh -c 'if ! grep -r --include \*.conf "\[{}\]" $AST_CONFIG_DIR/; then echo "Missing reference:{}"; fi' | grep "Missing reference:" | cut -d: -f2 | xargs -n1 -I{} grep -rn --include \*.conf "{}" $AST_CONFIG_DIR/ | sed 's/\s\s*/ /g' | cut -c 15- | grep -v ":;")
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
	results=$(pcregrep -ro -hi --include='.*\.conf$' '^\[([A-Za-z0-9-])+?\]' $(ls -d $AST_CONFIG_DIR/*/) | cut -d "[" -f2 | cut -d "]" -f1 | xargs -n1 -I{} sh -c 'if ! grep -rE --include \*.conf "([ @?,:^\(=]){}"  $AST_CONFIG_DIR/; then echo "Unreachable Code:{}"; fi' | grep "Unreachable Code:" | grep -vE "\-([0-9]+)$" | cut -d: -f2 | xargs -n1 -I{} grep -rn --include \*.conf "{}" $(ls -d $AST_CONFIG_DIR/*/) | sed 's/\s\s*/ /g' | cut -c 15- | grep -v ":;")
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

# Minimum argument check
if [ $# -lt $MIN_ARGS ]; then
	cmd="help"
else
	cmd="$1"
fi

FLAG_TEST=0
PARSED_ARGUMENTS=$(getopt -n phreaknet -o bc:u:dfhostu:v:w -l backtraces,cc:,dahdi,force,flag-test,help,sip,testsuite,user:,version:,weaktls,cisco,sccp,clli:,debug:,disa:,api-key:,rotate,boilerplate,upstream: -- "$@")
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
		-f | --force ) FORCE_INSTALL=1; shift ;;
		-h | --help ) cmd="help"; shift ;;
		-o | --flag-test ) FLAG_TEST=1; shift;;
		-s | --sip ) CHAN_SIP=1; shift ;;
		-t | --testsuite ) TEST_SUITE=1; shift ;;
		-u | --user ) AST_USER=$2; shift 2;;
		-v | --version ) AST_ALT_VER=$2; shift 2;;
		-w | --weaktls ) WEAK_TLS=1; shift ;;
		--cisco ) SIP_CISCO=1; shift ;;
		--sccp ) CHAN_SCCP=1; shift ;;
		--boilerplate ) BOILERPLATE_SOUNDS=1; shift ;;
		--clli ) PHREAKNET_CLLI=$2; shift 2;;
		--disa ) PHREAKNET_DISA=$2; shift 2;;
		--debug ) DEBUG_LEVEL=$2; shift 2;;
		--api-key ) INTERLINKED_APIKEY=$2; shift 2;;
		--rotate ) ASTKEYGEN=1; shift ;;
		--upstream ) SCRIPT_UPSTREAM=$2; shift 2;;
		# -- means the end of the arguments; drop this, and break out of the while loop
		--) shift; break ;;
		# If invalid options were passed, then getopt should have reported an error,
		# which we checked as VALID_ARGUMENTS when getopt was called...
		*) echo "Unexpected option: $1"
		   cmd="help"; shift; break ;;
	esac
done

if [ "$FLAG_TEST" = "1" ]; then
	echog "Flag test successful."
	exit 0
fi

if [ -z "${AST_CC##*[!0-9]*}" ] ; then # POSIX compliant: https://unix.stackexchange.com/a/574169/
	echoerr "Country code must be an integer."
	exit 1
fi

if [ "$cmd" = "help" ]; then
	usage
	exit 2
elif [ "$cmd" = "make" ]; then
	# chmod +x phreaknet.sh
	# ./phreaknet.sh make
	# phreaknet install

	if [ $(id -u) -ne 0 ]; then
		echoerr "PhreakScript make must be run as root. Aborting..."
		exit 2
	fi
	ln $FILE_PATH /usr/local/sbin/phreaknet
	if [ $? -eq 0 ]; then
		echo "PhreakScript added to path."
	else
		echo "PhreakScript could not be added to path. Is it already there?"
		echo "If it's not, move the source file (phreaknet.sh) to /usr/local/src and try again"
	fi
elif [ "$cmd" = "install" ]; then
	if [ $(id -u) -ne 0 ]; then
		echoerr "PhreakScript install must be run as root. Aborting..."
		exit 2
	fi
	freediskspace=`df --local --type=ext4 | head -2 | tail -1 | awk '{print $4}'` # free disk space, in KB
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
	# Install Pre-Reqs
	printf "%s %d\n" "Starting installation with country code" $AST_CC
	if [ -f "/etc/init.d/mysql" ]; then
		if [ "$FORCE_INSTALL" = "1" ]; then
			printf "%s\n" "Restarting MySQL/MariaDB service..."
			# mysql uses up a lot of CPU and memory, and will kill/really slow down the compilation process
			service mysql restart
		fi
	fi
	printf "%s\n" "Installing prerequisites..."
	install_prereq
	# Get DAHDI
	if [ "$CHAN_DAHDI" = "1" ]; then
		apt-get install -y linux-headers-`uname -r` build-essential binutils-dev
		if [ $? -ne 0 ]; then
			echoerr "Failed to download system headers"
			exit 2
		fi
		cd $AST_SOURCE_PARENT_DIR
		wget -q --show-progress http://downloads.asterisk.org/pub/telephony/dahdi-linux/dahdi-linux-current.tar.gz
		wget -q --show-progress http://downloads.asterisk.org/pub/telephony/dahdi-tools/dahdi-tools-current.tar.gz
		DAHDI_LIN_SRC_DIR=`tar -tzf dahdi-linux-current.tar.gz | head -1 | cut -f1 -d"/"`
		DAHDI_TOOLS_SRC_DIR=`tar -tzf dahdi-tools-current.tar.gz | head -1 | cut -f1 -d"/"`
		if [ -d "$AST_SOURCE_PARENT_DIR/$DAHDI_LIN_SRC_DIR" ]; then
			if [ "$FORCE_INSTALL" = "1" ]; then
				rm -rf $DAHDI_LIN_SRC_DIR
			else
				echoerr "Directory $DAHDI_LIN_SRC_DIR already exists. Please rename or delete this directory and restart installation or specify the force flag."
				exit 1
			fi
		fi
		if [ -d "$AST_SOURCE_PARENT_DIR/$DAHDI_TOOLS_SRC_DIR" ]; then
			if [ "$FORCE_INSTALL" = "1" ]; then
				rm -rf $DAHDI_TOOLS_SRC_DIR
			else
				echoerr "Directory $DAHDI_TOOLS_SRC_DIR already exists. Please rename or delete this directory and restart installation or specify the force flag."
				exit 1
			fi
		fi
		tar -zxvf dahdi-linux-current.tar.gz && rm dahdi-linux-current.tar.gz
		tar -zxvf dahdi-tools-current.tar.gz && rm dahdi-tools-current.tar.gz
		if [ ! -d $AST_SOURCE_PARENT_DIR/$DAHDI_LIN_SRC_DIR ]; then
			printf "Directory not found: %s\n" "$AST_SOURCE_PARENT_DIR/$DAHDI_LIN_SRC_DIR"
			exit 2
		fi
		cd $AST_SOURCE_PARENT_DIR/$DAHDI_LIN_SRC_DIR
		dahdi_unpurge $DAHDI_LIN_SRC_DIR
		# these bring the master branch up to the next branch (alternate to git clone git://git.asterisk.org/dahdi/linux dahdi-linux -b next)
		dahdi_patch "45ac6a30f922f4eef54c0120c2a537794b20cf5c"
		dahdi_patch "6e5197fed4f3e56a45f7cf5085d2bac814269807"
		dahdi_patch "ac300cd895160c8d292e1079d6bf95af5ab23874"
		dahdi_patch "c98f59eead28cf66b271b031288542e34e603c43"
		dahdi_patch "34b9c77c9ab2794d4e912461e4c1080c4b1f6184"
		dahdi_patch "26fb7c34cba98c08face72cf29b70dfdc71449c6"
		dahdi_patch "90e8a54e3a482c3cee6afc6b430bb0aab7ee8f34"
		dahdi_patch "97e744ad9604bd7611846da0b9c0c328dc80f262"
		dahdi_patch "4df746fe3ffd6678f36b16c9b0750fa552da92e4"
		dahdi_patch "6d4c748e0470efac90e7dc4538ff3c5da51f0169"
		dahdi_patch "d228a12f1caabdbcf15a757a0999e7da57ba374d"
		dahdi_patch "5c840cf43838e0690873e73409491c392333b3b8"
		make
		if [ $? -ne 0 ]; then
			echoerr "DAHDI Linux compilation failed, aborting install"
			exit 1
		fi
		make install
		make all install config
		if [ ! -d $AST_SOURCE_PARENT_DIR/$DAHDI_TOOLS_SRC_DIR ]; then
			printf "Directory not found: %s\n" "$AST_SOURCE_PARENT_DIR/$DAHDI_TOOLS_SRC_DIR"
			exit 2
		fi
		cd $AST_SOURCE_PARENT_DIR/$DAHDI_TOOLS_SRC_DIR
		dahdi_custom_patch "dahdi_cfg" "$DAHDI_TOOLS_SRC_DIR/dahdi_cfg.c" "https://raw.githubusercontent.com/InterLinked1/phreakscript/master/patches/dahdi_cfg.diff" # bug fix for buffer too small for snprintf. See https://issues.asterisk.org/jira/browse/DAHTOOL-89
		autoreconf -i
		./configure
		make
		if [ $? -ne 0 ]; then
			echoerr "DAHDI Tools compilation failed, aborting install"
			exit 1
		fi
		make install
		make config
		make install-config
		dahdi_genconf modules # in case not in path
		cat /etc/dahdi/modules
		# modprobe <listed>
		dahdi_genconf system # in case not in path
		dahdi_cfg # in case not in path
	fi
	# Get latest Asterisk LTS version
	cd $AST_SOURCE_PARENT_DIR
	if [ "$AST_ALT_VER" != "" ]; then
		AST_SOURCE_NAME="asterisk-$AST_ALT_VER"
		printf "%s\n" "Proceeding to install Asterisk $AST_ALT_VER..."
		echoerr "***************************** WARNING *****************************"
		printf "%s\n" "PhreakScript IS NOT TESTED WITH OLDER VERSIONS OF ASTERISK!!!"
		printf "%s\n" "Proceed at your own risk..."
		sleep 1
	fi
	rm -f $AST_SOURCE_NAME.tar.gz # the name itself doesn't guarantee that the version is the same
	if [ "$AST_ALT_VER" = "" ]; then # download latest bundled version
		wget -q --show-progress https://downloads.asterisk.org/pub/telephony/asterisk/$AST_SOURCE_NAME.tar.gz
	else
		wget -q --show-progress https://downloads.asterisk.org/pub/telephony/asterisk/releases/$AST_SOURCE_NAME.tar.gz
	fi
	if [ $? -ne 0 ]; then
		echoerr "Failed to download file: https://downloads.asterisk.org/pub/telephony/asterisk/$AST_SOURCE_NAME.tar.gz"
		if [ "$AST_ALT_VER" != "" ]; then
			printf "It seems Asterisk version %s does not exist...\n" "$AST_ALT_VER"
		fi
		exit 1
	fi
	if [ "$CHAN_SCCP" = "1" ]; then
		git clone https://github.com/chan-sccp/chan-sccp.git chan-sccp
	fi
	AST_SRC_DIR=`tar -tzf $AST_SOURCE_NAME.tar.gz | head -1 | cut -f1 -d"/"`
	if [ -d "$AST_SOURCE_PARENT_DIR/$AST_SRC_DIR" ]; then
		if [ "$FORCE_INSTALL" = "1" ]; then
			rm -rf $AST_SRC_DIR
		else
			echoerr "Directory $AST_SRC_DIR already exists. Please rename or delete this directory and restart installation or specify the force flag."
			exit 1
		fi
	fi
	printf "%s\n" "Installing production version $AST_SRC_DIR..."
	tar -zxvf $AST_SOURCE_NAME.tar.gz
	rm $AST_SOURCE_NAME.tar.gz
	if [ ! -d $AST_SOURCE_PARENT_DIR/$AST_SRC_DIR ]; then
		printf "Directory not found: %s\n" "$AST_SOURCE_PARENT_DIR/$AST_SRC_DIR"
		exit 2
	fi
	AST_SRC_DIR2=`ls -d -v */ | grep "^asterisk-" | tail -1`
	AST_SRC_DIR2=`printf "%s" "$AST_SRC_DIR2" | cut -d'/' -f1`
	if [ "$AST_SRC_DIR" != "$AST_SRC_DIR2" ]; then
		echoerr "Directory $AST_SRC_DIR2 conflicts with installation of $AST_SRC_DIR. Please rename or delete and restart installation."
		exit 2
	fi
	cd $AST_SOURCE_PARENT_DIR/$AST_SRC_DIR
	if [ "$SIP_CISCO" = "1" ]; then # ASTERISK-13145 (https://issues.asterisk.org/jira/browse/ASTERISK-13145)
		# https://usecallmanager.nz/patching-asterisk.html and https://github.com/usecallmanagernz/patches
		wget -q "https://raw.githubusercontent.com/usecallmanagernz/patches/master/asterisk/cisco-usecallmanager-18.9.0.patch" -O /tmp/$CISCO_CM_SIP.patch
		patch --strip=1 < /tmp/$CISCO_CM_SIP.patch
		if [ $? -ne 0 ]; then
			echoerr "WARNING: Call Manager patch may have failed to apply correctly"
		fi
		rm /tmp/$CISCO_CM_SIP.patch
		CFLAGS="-DENABLE_SRTP_AES_GCM -DENABLE_SRTP_AES_256" ./configure
	fi
	# Install Pre-Reqs
	if [ "$PAC_MAN" = "apt-get" ]; then
		printf "%s %d" "libvpb1 libvpb1/countrycode string" "$AST_CC" | debconf-set-selections -v
		apt-get install -y libvpb1
	fi
	./contrib/scripts/install_prereq install
	./contrib/scripts/get_mp3_source.sh
	if [ "$TEST_SUITE" = "1" ]; then
		./configure --with-jansson-bundled --enable-dev-mode
	else
		./configure --with-jansson-bundled
	fi
	if [ $? -ne 0 ]; then
		exit 2
	fi
	cp contrib/scripts/voicemailpwcheck.py /usr/local/bin
	chmod +x /usr/local/bin/voicemailpwcheck.py
	# Change Compile Options: https://wiki.asterisk.org/wiki/display/AST/Using+Menuselect+to+Select+Asterisk+Options
	make menuselect.makeopts
	menuselect/menuselect --enable format_mp3 menuselect.makeopts # add mp3 support
	# We want ulaw, not gsm (default)
	menuselect/menuselect --disable-category MENUSELECT_MOH --disable-category MENUSELECT_CORE_SOUNDS --disable-category MENUSELECT_EXTRA_SOUNDS menuselect.makeopts
	# Only grab sounds if this is the first time
	if [ ! -d "$AST_SOUNDS_DIR" ]; then
		menuselect/menuselect --enable CORE-SOUNDS-EN-ULAW --enable MOH-OPSOUND-ULAW --enable EXTRA-SOUNDS-EN-ULAW menuselect.makeopts # get the ulaw audio files...
	fi
	if [ "$ENABLE_BACKTRACES" = "1" ]; then
		menuselect/menuselect --enable DONT_OPTIMIZE --enable BETTER_BACKTRACES menuselect.makeopts
	fi
	if [ "$TEST_SUITE" = "1" ]; then
		configure_devmode
	fi
	if [ "$CHAN_DAHDI" = "1" ]; then
		menuselect/menuselect --enable chan_dahdi menuselect.makeopts
	fi
	if [ "$CHAN_SIP" = "1" ]; then # somebody still wants chan_sip, okay...
		echoerr "chan_sip is deprecated and will be removed in Asterisk 21. Please migrate to chan_pjsip at your convenience."
		menuselect/menuselect --enable chan_sip menuselect.makeopts
	else
		menuselect/menuselect --disable chan_sip menuselect.makeopts # remove this in version 21
	fi
	# in 19+, ADSI is not built by default. We should always build and enable it.
	menuselect/menuselect --enable res_adsi --enable app_adsiprog --enable app_getcpeid menuselect.makeopts
	# Disable things that were deprecated in 16 and were already removed in 19. These will not exist in the next LTS, so if anyone is using them, make 'em aware of it.
	menuselect/menuselect --disable res_monitor --disable app_url --disable app_image --disable app_ices --disable chan_oss --disable app_dahdiras --disable app_nbscat menuselect.makeopts
	# Disable the built-in skinny and mgcp modules, since there are better alternatives, and they're deprecated as of 19
	menuselect/menuselect --disable chan_skinny --disable chan_mgcp menuselect.makeopts
	# Who's actually using this?
	menuselect/menuselect --disable app_osplookup menuselect.makeopts
	# Expand TLS support from 1.2 to 1.0 for older ATAs, if needed
	if [ "$WEAK_TLS" = "1" ]; then
		sed -i 's/TLSv1.2/TLSv1.0/g' /etc/ssl/openssl.cnf
		sed -i 's/DEFAULT@SECLEVEL=2/DEFAULT@SECLEVEL=1/g' /etc/ssl/openssl.cnf
		printf "%s\n" "Successfully patched OpenSSL to allow TLS 1.0..."
	fi
	# Add PhreakNet patches
	printf "%s\n" "Beginning custom patches..."
	phreak_patches $PATCH_DIR $AST_SRC_DIR # custom patches
	printf "%s\n" "Custom patching completed..."
	if [ "$OS_DIST_INFO" = "FreeBSD" ]; then
		freebsd_port_patches
	fi
	# Compile Asterisk
	if [ "$AST_MAKE" = "gmake" ]; then # jansson fails to compile nicely on its own with gmake
		cd third-party/jansson
		gmake
		cd ../..
	fi
	if [ "$OS_DIST_INFO" = "FreeBSD" ]; then
		nice $AST_MAKE ASTLDFLAGS=-lcrypt main
	else
		nice $AST_MAKE # compile Asterisk. This is the longest step, if you are installing for the first time. Also, don't let it take over the server.
	fi

	if [ $? -ne 0 ]; then
		if [ "$TEST_SUITE" = "1" ]; then
			/usr/bin/xmlstarlet val -d doc/appdocsxml.dtd -e doc/core-en_US.xml # by default, it doesn't tell you whether the docs failed to validate. So if validation failed, print that out.
		fi
		exit 2
	fi
	if [ "$TEST_SUITE" = "1" ]; then
		$AST_MAKE full # some XML syntax warnings aren't shown unless make full is run
	fi
	if [ $? -ne 0 ]; then
		if [ "$TEST_SUITE" = "1" ]; then
			/usr/bin/xmlstarlet val -d doc/appdocsxml.dtd -e doc/core-en_US.xml # by default, it doesn't tell you whether the docs failed to validate. So if validation failed, print that out.
		fi
		exit 2
	fi

	# Only generate config if this is the first time
	# Install Asterisk
	if [ ! -d "$AST_CONFIG_DIR" ]; then
		$AST_MAKE samples # do not run this on upgrades or it will wipe out your config!
	fi
	if [ -d /usr/lib/asterisk/modules ]; then
		rm -f /usr/lib/asterisk/modules/*.so
	elif [ -d /usr/local/lib/asterisk/modules ]; then
		rm -f /usr/local/lib/asterisk/modules/*.so
	fi
	$AST_MAKE install # actually install modules
	if [ "$OS_DIST_INFO" = "FreeBSD" ]; then
		for FILE in $(find /usr/local/lib/asterisk/modules -name "*.so" ) ; do
			nm -D ${FILE} | grep PJ_AF_INET
		done
	fi
	$AST_MAKE config # install init script
	if [ "$OS_DIST_INFO" = "FreeBSD" ]; then
		wget https://raw.githubusercontent.com/freebsd/freebsd-ports/7abe6caf38ac7e552642372be9b4136c4354d0fd/net/asterisk18/files/asterisk.in
		cp asterisk.in /usr/local/etc/rc.d/asterisk
		chmod +x /usr/local/etc/rc.d/asterisk
	fi
	$AST_MAKE install-logrotate # auto compress and rotate log files

	if [ "$ODBC" = "1" ]; then # MariaDB ODBC for Asterisk
		apt-get install -y unixodbc unixodbc-dev mariadb-server
		cd $AST_SOURCE_PARENT_DIR
		wget https://downloads.mariadb.com/Connectors/odbc/connector-odbc-3.1.11/mariadb-connector-odbc-3.1.11-debian-buster-amd64.tar.gz # https://downloads.mariadb.com/Connectors/odbc/connector-odbc-3.1.11/
		tar -zxvf mariadb-connector-odbc-3.1.11-debian-buster-amd64/lib/mariadb
		cd mariadb-connector-odbc-3.1.11-debian-buster-amd64/lib/mariadb
		cp libmaodbc.so /usr/lib/x86_64-linux-gnu/odbc/
		cp libmariadb.so /usr/lib/x86_64-linux-gnu/odbc/
		cd $AST_SOURCE_PARENT_DIR
		odbcinst -j
		echo "[MariaDB]" > mariadb.ini && echo "Description	= Maria DB" >> mariadb.ini && echo "Driver = /usr/lib/x86_64-linux-gnu/odbc/libmaodbc.so" >> mariadb.ini && echo "Setup = /usr/lib/x86_64-linux-gnu/odbc/libodbcmyS.so" >> mariadb.ini
		odbcinst -i -d -f mariadb.ini
		rm mariadb.ini
	fi

	if [ ${#AST_USER} -gt 0 ]; then
		id -u asterisk
		# Create a user, if needed
		if [ $? -ne 0 ]; then
			adduser -c "Asterisk" $AST_USER --disabled-password --gecos ""
		fi
		# su - asterisk
		sed -i 's/ASTARGS=""/ASTARGS="-U $AST_USER"/g' /sbin/safe_asterisk
		sed -i 's/#AST_USER="asterisk"/AST_USER="$AST_USER"/g' /etc/default/asterisk
		sed -i 's/#AST_GROUP="asterisk"/AST_GROUP="$AST_USER"/g' /etc/default/asterisk
		chown -R $AST_USER $AST_CONFIG_DIR/ /usr/lib/asterisk /var/spool/asterisk/ $AST_VARLIB_DIR/ /var/run/asterisk/ /var/log/asterisk /usr/sbin/asterisk
	fi

	if [ "$CHAN_SCCP" = "1" ]; then
		cd $AST_SOURCE_PARENT_DIR
		cd chan-sccp
		if [ $? -eq 0 ]; then
			git fetch
			git pull
			./configure --enable-conference --enable-advanced-functions --with-asterisk=../$AST_SRC_DIR
			make -j2 && make install && make reload
			if [ $? -ne 0 ]; then
				echoerr "Failed to install chan_sccp"
				exit 2
			fi
		fi
	fi

	# Development Mode (Asterisk Test Suite)
	if [ "$TEST_SUITE" = "1" ]; then
		if [ "$PAC_MAN" = "apt-get" ]; then
			apt-get install gdb # for astcoredumper
		fi
		install_testsuite_itself
	fi

	/etc/init.d/asterisk status
	/etc/init.d/asterisk start
	/etc/init.d/asterisk status

	printf "%s\n" "Asterisk installation has completed. You may now connect to the Asterisk CLI: asterisk -r"
	printf "%s\n" "If you upgraded Asterisk, you will need to run 'core restart now' for the new version to load."
	if [ "$CHAN_DAHDI" = "1" ]; then
		echog "Note that DAHDI was installed and requires a reboot before it can be used."
	fi
elif [ "$cmd" = "pulsar" ]; then
	cd $AST_SOURCE_PARENT_DIR
	wget https://octothorpe.info/downloads/pulsar-agi.tar.gz
	wget https://code.phreaknet.org/asterisk/pulsar-noanswer.agi # bug fix to pulsar.agi, to fix broken answer supervision:
	mv pulsar-agi.tar.gz $AST_VARLIB_DIR
	cd $AST_VARLIB_DIR
	tar xvfz pulsar-agi.tar.gz # automatically creates $AST_VARLIB_DIR/sounds/pulsar/
	mv $AST_SOURCE_PARENT_DIR/pulsar-noanswer.agi $AST_VARLIB_DIR/agi-bin/pulsar.agi
	chmod 755 $AST_VARLIB_DIR/agi-bin/pulsar.agi
	if [ ! -f $AST_VARLIB_DIR/agi-bin/pulsar.agi ]; then
		echoerr "pulsar.agi is missing"
	fi
elif [ "$cmd" = "sounds" ]; then
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
	fi
elif [ "$cmd" = "ulaw" ]; then
	if ! which sox > /dev/null; then
		if [ "$PAC_MAN" = "apt-get" ]; then
			apt-get install -y sox
		fi
	fi
	if [ ${#2} -gt 0 ]; then # a specific file
		withoutextension=`printf '%s\n' "$2" | sed -r 's|^(.*?)\.\w+$|\1|'` # see comment on https://stackoverflow.com/a/26753382
		sox "$2" --rate 8000 --channels 1 --type ul $withoutextension.ulaw lowpass 3400 highpass 300
	else
		for f in *.wav; do
			withoutextension=`printf '%s\n' "$f" | sed -r 's|^(.*?)\.\w+$|\1|'`
			sox $f --rate 8000 --channels 1 --type ul $withoutextension.ulaw lowpass 3400 highpass 300
		done
	fi
elif [ "$cmd" = "installts" ]; then
	if [ $(id -u) -ne 0 ]; then
		echoerr "PhreakScript installts must be run as root. Aborting..."
		exit 2
	fi
	install_testsuite "$FORCE_INSTALL"
elif [ "$cmd" = "docverify" ]; then
	/usr/bin/xmlstarlet val -d doc/appdocsxml.dtd -e doc/core-en_US.xml # show the XML validation errors
	/usr/bin/xmlstarlet val -d doc/appdocsxml.dtd -e doc/core-en_US.xml 2>&1 | grep "doc/core-en_US.xml:" | cut -d':' -f2 | cut -d'.' -f1 | xargs  -d "\n" -I{} sed "{}q;d" doc/core-en_US.xml # show the offending lines
elif [ "$cmd" = "gerrit" ]; then
	read -r -p "Gerrit Patchset: " gurl
	phreak_gerrit_off "$gurl"
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
	cd $AST_SOURCE_PARENT_DIR
	AST_SRC_DIR=`ls -d -v */ | grep "^asterisk-" | tail -1`
	cd $AST_SRC_DIR
	if [ $? -ne 0 ]; then
		echoerr "Couldn't find Asterisk source directory?"
		exit 2
	fi
	if [ ! -f doc/core-en_US.xml ]; then
		printf "%s\n" "Failed to find any XML documentation. Has Asterisk been installed yet?"
		exit 2
	fi
	/usr/bin/xmlstarlet val -d doc/appdocsxml.dtd -e doc/core-en_US.xml
	if [ $? -ne 0 ]; then
		exit 2 # if the XML docs aren't valid, then give up now
	fi
	if [ "$PAC_MAN" = "apt-get" ]; then
		apt-get install -y php php7.3-xml
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
elif [ "$cmd" = "pubdocs" ]; then
	cd $AST_SOURCE_PARENT_DIR
	AST_SRC_DIR=`ls -d -v */ | grep "^asterisk-" | tail -1`
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
		ls -d -v $AST_SOURCE_PARENT_DIR/*/ | grep "asterisk-" | tail -1
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
	printf "%s\n" "phreaknet pubdocs is deprecated. Please migrate to phreaknet docgen instead"
elif [ "$cmd" = "config" ]; then
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
		exit 2
	fi
	if [ ${#PHREAKNET_DISA} -ne 7 ]; then
		echoerr "PhreakNet DISA number must be 7 digits."
		exit 2
	fi
	printf "%s: %s\n" "PhreakNet CLLI code" $PHREAKNET_CLLI
	printf "%s\n" "InterLinked API key seems to be of valid format, not displaying for security reasons..."
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
	cp $AST_CONFIG_DIR/*.conf /tmp/etc/_asterisk # do we really trust users not to make a mistake? backup to tmp, at least...
	printf "%s" "Installing boilerplate code to "
	pwd
	printf "\n"
	wget -q --show-progress https://docs.phreaknet.org/verification.conf -O verification.conf --no-cache
	wget -q --show-progress https://docs.phreaknet.org/phreaknet.conf -O phreaknet.conf --no-cache
	wget -q --show-progress https://docs.phreaknet.org/phreaknet-aux.conf -O phreaknet-aux.conf --no-cache
	ls -l
	cd $AST_CONFIG_DIR
	wget -q --show-progress https://docs.phreaknet.org/asterisk.conf -O $AST_CONFIG_DIR/asterisk.conf --no-cache
	wget -q --show-progress https://docs.phreaknet.org/iax.conf -O $AST_CONFIG_DIR/iax.conf --no-cache
	wget -q --show-progress https://docs.phreaknet.org/sip.conf -O $AST_CONFIG_DIR/sip.conf --no-cache
	wget -q --show-progress https://docs.phreaknet.org/pjsip.conf -O $AST_CONFIG_DIR/pjsip.conf --no-cache
	wget -q --show-progress https://docs.phreaknet.org/musiconhold.conf -O $AST_CONFIG_DIR/musiconhold.conf --no-cache
	wget -q --show-progress https://docs.phreaknet.org/extensions.conf -O $AST_CONFIG_DIR/extensions.conf --no-cache
	wget -q --show-progress https://docs.phreaknet.org/verify.conf -O $AST_CONFIG_DIR/verify.conf --no-cache
	## Inject user config (CLLI code, API key)
	sed -i "s/abcdefghijklmnopqrstuvwxyz/$INTERLINKED_APIKEY/g" $AST_CONFIG_DIR/extensions.conf
	sed -i "s/WWWWXXYYZZZ/$PHREAKNET_CLLI/g" $AST_CONFIG_DIR/extensions.conf
	sed -i "s/5551111/$PHREAKNET_DISA/g" $AST_CONFIG_DIR/extensions.conf
	printf "Updated [globals] in %s/extensions.conf with dynamic variables. If globals are stored in a different file, manual updating is required." $AST_CONFIG_DIR
	printf "%s\n" "Boilerplate config installed! Note that these files may still require manual editing before use."
elif [ "$cmd" = "keygen" ]; then
	if [ ${#INTERLINKED_APIKEY} -eq 0 ]; then
		INTERLINKED_APIKEY=`grep -R "interlinkedkey=" $AST_CONFIG_DIR | grep -v "your-interlinked-api-key" | cut -d'=' -f2 | awk '{print $1;}'`
		if [ ${#INTERLINKED_APIKEY} -lt 30 ]; then
			echoerr "Failed to find InterLinked API key. For future use, please set your [global] variables, e.g. by running phreaknet config"
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
			echoerr "Failed to find PhreakNet CLLI. For future use, please set your [global] variables, e.g. by running phreaknet config"
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
	if [ ${#INTERLINKED_APIKEY} -gt 30 ] && [ ${#PHREAKNET_CLLI} -eq 11 ] ; then
		wanip=`dig +short myip.opendns.com @resolver4.opendns.com`
		printf "%s %s\n" "Autodetected WAN IP address is" $wanip
		wanfqdn=`dig +short -x $wanip`
		printf "%s %s\n" "Autodetected WAN FQDN hostname is" $wanfqdn
		printf "%s\n" "Attempting to upload RSA public key..."
		cat phreaknetrsa.pub | curl -X POST --data-urlencode "rsakey=$(</dev/stdin)" -d "key=$INTERLINKED_APIKEY" -d "clli=$PHREAKNET_CLLI" https://api.phreaknet.org/v1/rsa
		printf "\n"
	else
		printf "%s\n" "No InterLinked API key and/or CLLI on file, skipping upload of public key..."
	fi
	touch $AST_CONFIG_DIR/iax-phreaknet-rsa-in.conf
	# touch $AST_CONFIG_DIR/iax-phreaknet-rsa-out.conf # no longer necessary for >= 18.9
	## If you are running Asterisk as not root, make the user as which Asterisk runs own the private key and the new files:
	# chown asterisk phreaknetrsa.key
	# chown asterisk $AST_CONFIG_DIR/iax-phreaknet*
	asterisk -rx "module reload res_crypto"
	asterisk -rx "keys init"
	asterisk -rx "keys show"
elif [ "$cmd" = "update" ]; then
	if [ ! -f "/tmp/phreakscript_update.sh" ]; then
		wget -q $PATCH_DIR/phreakscript_update.sh -O /tmp/phreakscript_update.sh
		chmod +x /tmp/phreakscript_update.sh
	fi
	printf "%s\n" "Updating PhreakScript..."
	exec /tmp/phreakscript_update.sh "$SCRIPT_UPSTREAM" "$FILE_PATH" "/tmp/phreakscript_update.sh"
elif [ "$cmd" = "patch" ]; then
	printf "%s\n" "Updating PhreakNet Asterisk configuration..."
	rm -f /tmp/phreakpatch/*.patch
	rm -rf /tmp/phreakpatch
	mkdir /tmp/phreakpatch
	printf "%s\n" "Downloading patches..."
	wget -r -l 1 -nd -np -q --show-progress -P /tmp/phreakpatch -R -A patch "https://code.phreaknet.org/asterisk/patches/"
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
elif [ "$cmd" = "edit" ]; then
	exec nano $FILE_PATH
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
	paste_post "/var/log/asterisk/$channel"
	rm /var/log/asterisk/$channel
elif [ "$cmd" = "pcap" ]; then
	apt-get install -y tshark
	debugtime=$EPOCHSECONDS
	if [ "$debugtime" = "" ]; then
		debugtime=`awk 'BEGIN {srand(); print srand()}'` # https://stackoverflow.com/a/41324810
	fi
	ip=""
	if [ ${#2} -gt 0 ]; then
		ip=$2
		printf "%s\n" "Starting packet capture for IP $ip - press CTRL+C to conclude"
		tshark -f "not port 22 and host $ip" -i any -w /tmp/pcap_$debugtime.pcap
	else
		printf "%s\n" "Starting packet capture (all IPs) - press CTRL+C to conclude"
		tshark -f "not port 22" -i any -w /tmp/pcap_$debugtime.pcap
	fi
	printf "Packet capture successfully saved to %s\n" "/tmp/pcap_$debugtime.pcap"
elif [ "$cmd" = "enable-backtraces" ]; then
	cd $AST_SOURCE_PARENT_DIR
	AST_SRC_DIR=`ls -d -v */ | grep "^asterisk-" | tail -1`
	cd $AST_SRC_DIR
	make menuselect.makeopts
	menuselect/menuselect --enable DONT_OPTIMIZE --enable BETTER_BACKTRACES menuselect.makeopts
	make
	make install
elif [ "$cmd" = "valgrind" ]; then # https://wiki.asterisk.org/wiki/display/AST/Valgrind
	cd $AST_SOURCE_PARENT_DIR
	AST_SRC_DIR=`ls -d -v */ | grep "^asterisk-" | tail -1`
	cd /tmp
	asterisk -rx "core stop now"
	sleep 1
	valgrind --suppressions=$AST_SOURCE_PARENT_DIR/${AST_SRC_DIR}contrib/valgrind.supp --log-fd=9 asterisk -vvvvcg 9 > asteriskvalgrind.txt
	ls /tmp/asteriskvalgrind.txt
elif [ "$cmd" = "backtrace-only" ]; then
	get_backtrace 0
elif [ "$cmd" = "backtrace" ]; then
	get_backtrace 1
elif [ "$cmd" = "ccache" ]; then
	cd $AST_SOURCE_PARENT_DIR
	wget https://github.com/ccache/ccache/releases/download/v4.5.1/ccache-4.5.1.tar.gz
	if [ $? -ne 0 ]; then
		exit 1
	fi
	tar -zxvf ccache-4.5.1.tar.gz
	cd ccache-4.5.1
	cmake -DCMAKE_BUILD_TYPE=Release -DZSTD_FROM_INTERNET=ON -DHIREDIS_FROM_INTERNET=ON ..
	make
	make install
	if [ $? -ne 0 ]; then
		exit 1
	fi
	which ccache
	ln -s ccache /usr/local/bin/gcc
	ln -s ccache /usr/local/bin/g++
	ln -s ccache /usr/local/bin/cc
	ln -s ccache /usr/local/bin/c++
	which gcc
elif [ "$cmd" = "examples" ]; then
	printf "%s\n\n" 	"========= PhreakScript Example Usages ========="
	printf "%s\n\n" 	"Presented in the logical order of usage, but with multiple variations for each command."
	printf "%s\n\n" 	"phreaknet update                   Update PhreakScript. No Asterisk or configuration modification will occur."
	printf "%s\n\n" 	"Installation commands:"
	printf "%s\n"		"phreaknet install                  Install the latest version of Asterisk."
	printf "%s\n"		"phreaknet install --cc=44          Install the latest version of Asterisk, with country code 44."
	printf "%s\n"		"phreaknet install --force          Reinstall the latest version of Asterisk."
	printf "%s\n"		"phreaknet install --dahdi          Install the latest version of Asterisk, with DAHDI."
	printf "%s\n"		"phreaknet install --sip --weaktls  Install Asterisk with chan_sip built AND support for TLS 1.0."
	printf "%s\n"		"phreaknet install --version 18.9.0 Install Asterisk version 18.9.0 as the base version of Asterisk."
	printf "%s\n"		"phreaknet installts                Install Asterisk Test Suite and Unit Test support (developers only)."
	printf "%s\n"		"phreaknet pulsar                   Install revertive pulsing pulsar AGI, with bug fixes"
	printf "%s\n"		"phreaknet sounds --boilerplate     Install Pat Fleet sounds and basic boilerplate old city tone audio."
	printf "%s\n"		"phreaknet config --force --api-key=<KEY> --clli=<CLLI> --disa=<DISA>"
	printf "%s\n"		"                                   Download and initialize boilerplate PhreakNet configuration"
	printf "%s\n"		"phreaknet keygen                   Upload existing RSA public key to PhreakNet"
	printf "%s\n"		"phreaknet keygen --rotate          Create or rotate PhreakNet RSA keypair, then upload public key to PhreakNet"
	printf "%s\n"		"phreaknet validate                 Validate your dialplan configuration and check for errors"
	printf "\n%s\n\n"	"Debugging commands:"
	printf "%s\n"		"phreaknet trace                    Perform a trace with verbosity 10 and no debug level"
	printf "%s\n"		"phreaknet trace --debug 1          Perform a trace with verbosity 10 and debug level 1"
	printf "%s\n"		"phreaknet backtrace      		    Process, extract, and upload a core dump"
	printf "\n%s\n\n"	"Maintenance commands:"
	printf "%s\n"		"phreaknet update                   Update PhreakScript. No Asterisk or configuration modification will occur."
	printf "%s\n"		"phreaknet update --upstream=URL    Update PhreakScript using URL as the upstream source (for testing)."
	printf "%s\n"		"phreaknet patch                    Apply the latest PhreakNet configuration patches."
	printf "\n"
elif [ "$cmd" = "info" ]; then
	phreakscript_info
elif [ "$cmd" = "about" ]; then
	printf "%s\n%s\n%s\n\n" "========= PhreakScript =========" "PhreakScript automates the management of Asterisk and DAHDI, from installation to patching to debugging." "The version of Asterisk and DAHDI installed by PhreakScript isn't a fork of Asterisk/DAHDI. Rather, it builds on top of the latest versions of Asterisk and DAHDI, so that users benefit from bug fixes and new features and improvements upstream, but also adds additional bug fixes and features that haven't made it upstream, to provide the fullest and richest Asterisk/DAHDI experience. For more details, see https://phreaknet.org/changes" | fold -s -w 80
	printf "%s\n" "Change Log (both changes to PhreakScript and to Asterisk/DAHDI as installed by PhreakScript"
	grep "^#" $FILE_PATH | grep -A 999999 "Begin Change Log" | grep -B 999999 "End Change Log" | tail -n +2 | head -n -1 | cut -c 3-
	printf "\n"
	phreakscript_info
else
	echoerr "Invalid command: $cmd, type phreaknet help for usage."
	exit 2
fi

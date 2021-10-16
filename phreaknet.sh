#!/bin/bash

# PhreakScript for Debian systems
# (C) 2021 PhreakNet - https://portal.phreaknet.org and https://docs.phreaknet.org
# v0.0.42 (2021-10-16)

# Setup (as root):
# cd /etc/asterisk/scripts
# wget https://docs.phreaknet.org/script/phreaknet.sh
# chmod +x phreaknet.sh
# ./phreaknet.sh make
# exec $SHELL
# phreaknet update
# phreaknet install

## Begin Change Log:
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
# 2021-10-07 0.0.1  Asterisk (ASTERISK-20219): chan_iax2 (Improvement): Add encryption to RSA authentication (merged as of Oct. 2021, patch will be removed soon) # TODO
## End Change Log

# Script environment variables
AST_SOURCE_NAME="asterisk-18-current"
AST_SOURCE_NAME="asterisk-18.8.0-rc1" # remove once 18.8 is released # TODO
MIN_ARGS=1
FILE_PATH=$(realpath -s $0)
PATCH_DIR=https://docs.phreaknet.org/script
BASH_RC="/etc/bash.bashrc"
AST_CONFIG_DIR="/etc/asterisk"
AST_SOUNDS_DIR="/var/lib/asterisk/sounds/en"
AST_MOH_DIR="/var/lib/asterisk/moh"

# Defaults
AST_CC=1 # Country Code (default: 1 - NANPA)
AST_USER=""
WEAK_TLS=0
CHAN_SIP=0
CHAN_DAHDI=0
TEST_SUITE=0
FORCE_INSTALL=0
ASTKEYGEN=0
DEFAULT_PATCH_DIR="/var/www/html/interlinked/sites/phreak/code/asterisk/patches" # for new patches
PHREAKNET_CLLI=""
INTERLINKED_APIKEY=""
BOILERPLATE_SOUNDS=0
SCRIPT_UPSTREAM="$PATCH_DIR/phreaknet.sh"

echog() { printf "\e[32;1m%s\e[0m\n" "$*" >&2; }
echoerr() { printf "\e[31;1m%s\e[0m\n" "$*" >&2; } # https://stackoverflow.com/questions/2990414/echo-that-outputs-to-stderr

phreakscript_info() {
	printf "%s" "PhreakScript "
	grep "# v" $FILE_PATH | head -1 | cut -d'v' -f2
	/sbin/asterisk -V
	if [ -d /etc/dahdi ]; then
		/sbin/dahdi_cfg -tv 2>&1 | grep "ersion"
	fi
	echo "(C) 2021 PhreakNet - https://portal.phreaknet.org https://docs.phreaknet.org"
	echo "To report bugs or request feature additions, please report at https://issues.interlinked.us (also see https://docs.phreaknet.org/#contributions) and/or post to the PhreakNet mailing list: https://groups.io/g/phreaknet" | fold -s -w 120
}

usage() {
	complete -W "make help install installts pulsar sounds config keygen edit genpatch patch update upgrade trace backtrace" phreaknet # has to be manually entered, at present
	#source ~/.bash_profile
	echo "Usage: phreaknet [command] [options]
Commands:
   *** Getting Started ***
   about         About PhreakScript
   help          Program usage
   examples      Example usages

   *** First Use / Installation ***
   make          Add PhreakScript to path
   install       Install or upgrade PhreakNet Asterisk
   installts     Install Asterisk Test Suite
   pulsar        Install Revertive Pulsing simulator
   sounds        Install Pat Fleet sound library

   *** Initial Configuration ***
   config        Install PhreakNet boilerplate config
   keygen        Install and update PhreakNet RSA keys

   *** Maintenance ***
   update        Update PhreakScript
   upgrade       Upgrade PhreakNet Asterisk
   patch         Patch PhreakNet Asterisk configuration
   genpatch      Generate a PhreakPatch

   *** Debugging ***
   trace         Capture a CLI trace and upload to InterLinked Paste
   backtrace     Use astcoredumper to process a backtrace and upload to InterLinked Paste

   *** Miscellaneous ***
   pubdocs       Generate Wiki documentation
   edit          Edit PhreakScript

Options:
   -c, --cc           Country Code (default is 1)
   -d, --dahdi        Install DAHDI
   -f, --force        Force install or config
   -h, --help         Display usage
   -s, --sip          Use chan_sip instead of or in addition to chan_pjsip
   -t, --testsuite    Compile with developer support for Asterisk test suite and unit tests
   -u, --user         User as which to run Asterisk (non-root)
   -w, --weaktls      Allow less secure TLS versions down to TLS 1.0 (default is 1.2+)
       --boilerplate  sounds: Also install boilerplate sounds
       --clli         config: CLLI code
       --disa         config: DISA number
       --api-key      config: InterLinked API key
       --rotate       keygen: Rotate/create keys
	   --upstream     update: Specify upstream source
"
	phreakscript_info
	exit 2
}

install_prereq() {
	apt-get clean
	apt-get update -y
	apt-get upgrade -y
	apt-get dist-upgrade -y
	apt-get install -y ntp iptables tcpdump wget curl sox libcurl4-openssl-dev mpg123 dnsutils php festival bc fail2ban mariadb-server git
	# used to feed the country code non-interactively
	apt-get install -y debconf-utils
}

install_testsuite() {
	apt-get clean
	apt-get update -y
	apt-get upgrade -y
	apt-get install -y liblua5.1-0-dev lua5.3 git python python-setuptools
	cd /usr/src
	# Python 2 support is going away in Debian
	curl https://bootstrap.pypa.io/pip/2.7/get-pip.py -o get-pip.py # https://stackoverflow.com/a/64240018/6110631
	python get-pip.py
	pip2 install pyyaml
	pip2 install twisted
	# in case we're not already in the right directory
	AST_SRC_DIR=`ls /usr/src | grep "asterisk-" | tail -1`
	if [ "$AST_SRC_DIR" = ""]; then
		echoerr "Asterisk source not found. Aborting..."
		return 1
	fi
	cd /usr/src/$AST_SRC_DIR
	./configure --enable-dev-mode
	make menuselect.makeopts
	menuselect/menuselect --enable DONT_OPTIMIZE BETTER_BACKTRACES TEST_FRAMEWORK menuselect.makeopts
	make
	make install
	cd /usr/src
	git clone https://gerrit.asterisk.org/testsuite
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
	# ./runtests.py -t tests/channels/iax2/basic-call/ # run a single basic test
	printf "%s\n" "Asterisk Test Suite installation complete"
}

gerrit_patch() {
	printf "%s\n" "Downloading and applying Gerrit patch $1"
	wget -q --show-progress $2 -O $1.diff.base64
	base64 --decode $1.diff.base64 > $1.diff
	git apply $1.diff
	if [ $? -ne 0 ]; then
		echoerr "Failed to apply Gerrit patch... this should be reported..."
	fi
	rm $1.diff.base64 $1.diff
}

dahdi_undo() {
	printf "Restoring drivers by undoing PATCH: %s\n" "$3"
	wget -q --show-progress "https://git.asterisk.org/gitweb/?p=dahdi/linux.git;a=patch;h=$4" -O /tmp/$2.patch --no-cache
	patch -u -b -p 1 --reverse -i /tmp/$2.patch
	if [ $? -ne 0 ]; then
		echoerr "Failed to reverse DAHDI patch... this should be reported..."
	fi
	rm /tmp/$2.patch
}

dahdi_custom_patch() {
	printf "Applying custom DAHDI patch: %s\n" "$1"
	wget -q --show-progress "$3" -O /tmp/$1.patch --no-cache
	patch -u -b "/usr/src/$2" -i /tmp/$1.patch
	if [ $? -ne 0 ]; then
		echoerr "Failed to apply DAHDI patch... this should be reported..."
	fi
	rm /tmp/$1.patch
}

dahdi_patch() {
	printf "Applying unmerged DAHDI patch: %s\n" "$1"
	wget -q --show-progress "https://git.asterisk.org/gitweb/?p=dahdi/linux.git;a=patch;h=$1" -O /tmp/$1.patch --no-cache
	patch -u -b -p 1 -i /tmp/$1.patch
	if [ $? -ne 0 ]; then
		echoerr "Failed to apply DAHDI patch... this should be reported..."
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
	dahdi_custom_patch "wcte12xp_base" "$1/drivers/dahdi/wcte12xp/base.c" "https://code.phreaknet.org/asterisk/dahdi/wcte12xp_base.diff" # bug fix for case statement fallthrough
	dahdi_undo $1 "wcte11xp" "Remove support for wcte11xp." "3748456d22122cf807b47d5cf6e2ff23183f440d"
	dahdi_undo $1 "wctdm" "Remove support for wctdm." "04e759f9c5a6f76ed88bc6ba6446fb0c23c1ff55"
	dahdi_undo $1 "wct1xxp" "Remove support for wct1xxp." "dade6ac6154b58c4f5b6f178cc09de397359000b"
	dahdi_undo $1 "wcfxo" "Remove support for wcfxo." "14198aee8532bbafed2ad1297177f8e0e0f13f50"
	dahdi_undo $1 "tor2" "Remove support for tor2." "60d058cc7a064b6e07889f76dd9514059c303e0f"
	dahdi_undo $1 "pciradio" "Remove support for pciradio." "bfdfc4728c033381656b59bf83aa37187b5dfca8"
	printf "%s\n" "Finished undoing DAHDI removals!"
}

phreak_patches() { # $1 = $PATCH_DIR, $2 = $AST_SRC_DIR
	### Inject custom PhreakNet patches to add additional functionality and features.
	###  If/when/as these are integrated upstream, they will be removed from this function. 

	## Add Patches To Existing Modules

	cd /tmp
	wget https://code.phreaknet.org/asterisk/iaxrsa.patch
	wget https://code.phreaknet.org/asterisk/returnif.patch
	wget https://code.phreaknet.org/asterisk/6112308.diff
	wget https://issues.asterisk.org/jira/secure/attachment/60464/translate.diff

	cd /usr/src/$2

	# Add ReturnIf application
	patch -u -b apps/app_stack.c -i /tmp/returnif.patch
	# Bug fix to translation code
	patch -u -b main/translate.c -i /tmp/translate.diff
	# Bug fix to app_dial (prevent infinite loop)
	patch -u -b apps/app_dial.c -i /tmp/6112308.diff

	rm -f /tmp/iaxrsa.patch /tmp/returnif.patch /tmp/translate.diff /tmp/6112308.diff

	## Add Standalone Modules
	# Add app_if module
	wget https://code.phreaknet.org/asterisk/app_if.c -O /usr/src/$2/apps/app_if.c --no-cache
	# Add func_ochannel module
	wget https://code.phreaknet.org/asterisk/func_ochannel.c -O /usr/src/$2/funcs/func_ochannel.c --no-cache
	# Add func_notchfilter module
	wget https://code.phreaknet.org/asterisk/func_notchfilter.c -O /usr/src/$2/funcs/func_notchfilter.c --no-cache
	# Add app_mail module
	wget https://code.phreaknet.org/asterisk/app_mail.c -O /usr/src/$2/apps/app_mail.c --no-cache
	# Add app_memory module
	wget https://code.phreaknet.org/asterisk/app_memory.c -O /usr/src/$2/apps/app_memory.c --no-cache
	# Add func_evalexten module
	wget https://code.phreaknet.org/asterisk/func_evalexten.c -O /usr/src/$2/funcs/func_evalexten.c --no-cache

	## Additional Standalone Modules
	wget https://code.phreaknet.org/asterisk/app_frame.c -O /usr/src/$2/apps/app_frame.c --no-cache
	wget https://code.phreaknet.org/asterisk/app_streamsilence.c -O /usr/src/$2/apps/app_streamsilence.c --no-cache
	wget https://code.phreaknet.org/asterisk/app_tonetest.c -O /usr/src/$2/apps/app_tonetest.c --no-cache
	wget https://code.phreaknet.org/asterisk/func_dbchan.c -O /usr/src/$2/apps/func_dbchan.c --no-cache

	## Third Party Modules
	wget https://code.phreaknet.org/asterisk/app_softmodem.c -O /usr/src/$2/apps/app_softmodem.c --no-cache
	wget https://raw.githubusercontent.com/dgorski/app_tdd/main/app_tdd.c -O /usr/src/$2/apps/app_tdd.c --no-cache

	## Gerrit patches for merged in master branch (will be removed once released in next version)

	# never going to be merged upstream:
	gerrit_patch 16569 "https://gerrit.asterisk.org/changes/asterisk~16569/revisions/3/patch?download" # chan_sip: Add custom parameters

	## Menuselect updates
	make menuselect.makeopts
	menuselect/menuselect --enable app_memory menuselect.makeopts # app_memory doesn't compile by default
}

function download_if_missing() {
	if [ ! -f "$1" ]; then
		wget "$2"
	else
		printf "Audio file already present, not overwriting: %s\n" "$1"
	fi
}

# Minimum argument check
if [ $# -lt $MIN_ARGS ]; then
	cmd="help"
else
	cmd="$1"
fi

PARSED_ARGUMENTS=$(getopt -n phreaknet -o c:u:dfhstw -l cc:,dahdi,force,help,sip,testsuite,user:,weaktls,clli:,disa:,api-key:,rotate,boilerplate,upstream: -- "$@")
VALID_ARGUMENTS=$?
if [ "$VALID_ARGUMENTS" != "0" ]; then
	usage
	exit 2
fi

eval set -- "$PARSED_ARGUMENTS"
while true; do
	case "$1" in
        -c | --cc ) AST_CC=$2; shift 2;;
		-d | --dahdi ) CHAN_DAHDI=1; shift ;;
		-f | --force ) FORCE_INSTALL=1; shift ;;
		-h | --help ) cmd="help" ;;
		-s | --sip ) CHAN_SIP=1; shift ;;
		-t | --testsuite ) TEST_SUITE=1; shift ;;
		-u | --user ) AST_USER=$2; shift 2;;
		-w | --weaktls ) WEAK_TLS=1; shift ;;
		--boilerplate ) BOILERPLATE_SOUNDS=1; shift ;;
		--clli ) PHREAKNET_CLLI=$2; shift 2;;
		--disa ) PHREAKNET_DISA=$2; shift 2;;
		--api-key ) INTERLINKED_APIKEY=$2; shift 2;;
		--rotate ) ASTKEYGEN=1; shift ;;
		--upstream ) SCRIPT_UPSTREAM=$2; shift 2;;
		# -- means the end of the arguments; drop this, and break out of the while loop
		--) shift; break ;;
		# If invalid options were passed, then getopt should have reported an error,
		# which we checked as VALID_ARGUMENTS when getopt was called...
		*) echo "Unexpected option: $1"
		   cmd="help" ;;
	esac
done

if ! [[ "$AST_CC" =~ ^[0-9]+$ ]]; then
	echoerr "Country code must be an integer."
	exit 1
fi

if [ "$cmd" = "help" ]; then
	usage
	exit 2
elif [ "$cmd" = "make" ]; then
	# chmod +x phreaknet.sh
	# ./phreaknet.sh make
	# exec $SHELL
	# phreaknet install

	if [ "$EUID" -ne 0 ]; then
		echoerr "PhreakScript make must be run as root. Aborting..."
		exit 2
	fi
	v=`grep "phreaknet" $BASH_RC`
	if [ "$v" = "" ]; then
		printf "%s\n" "alias phreaknet='$FILE_PATH'" >> $BASH_RC
		printf "%s\n" "Aliased PhreakScript globally as phreaknet."
	else
		printf "%s\n" "PhreakScript already aliased, taking no action."
	fi
	# source ~/.bash_profile
	# exec $SHELL
elif [ "$cmd" = "install" ]; then
	if [ "$EUID" -ne 0 ]; then
		echoerr "PhreakScript install must be run as root. Aborting..."
		exit 2
	fi
	# Install Pre-Reqs
	printf "%s %d\n" "Starting installation with country code" $AST_CC
	if [ -f "/etc/init.d/mysql" ]; then
		printf "%s\n" "Restarting MySQL/MariaDB service..."
		# mysql uses up a lot of CPU and memory, and will kill the compilation process
		service mysql restart
	fi
	printf "%s\n" "Installing prerequisites..."
	install_prereq
	# Get DAHDI
	if [ "$CHAN_DAHDI" = "1" ]; then
		apt-get install -y linux-headers-`uname -r` build-essential binutils-dev
		cd /usr/src
		wget -q --show-progress http://downloads.asterisk.org/pub/telephony/dahdi-linux/dahdi-linux-current.tar.gz
		wget -q --show-progress http://downloads.asterisk.org/pub/telephony/dahdi-tools/dahdi-tools-current.tar.gz
		DAHDI_LIN_SRC_DIR=`tar -tzf dahdi-linux-current.tar.gz | head -1 | cut -f1 -d"/"`
		DAHDI_TOOLS_SRC_DIR=`tar -tzf dahdi-tools-current.tar.gz | head -1 | cut -f1 -d"/"`
		if [ -d "/usr/src/$DAHDI_LIN_SRC_DIR" ]; then
			if [ "$FORCE_INSTALL" = "1" ]; then
				rm -rf $DAHDI_LIN_SRC_DIR
			else
				echoerr "Directory $DAHDI_LIN_SRC_DIR already exists. Please rename or delete this directory and restart installation or specify the force flag."
				exit 1
			fi
		fi
		if [ -d "/usr/src/$DAHDI_TOOLS_SRC_DIR" ]; then
			if [ "$FORCE_INSTALL" = "1" ]; then
				rm -rf $DAHDI_TOOLS_SRC_DIR
			else
				echoerr "Directory $DAHDI_TOOLS_SRC_DIR already exists. Please rename or delete this directory and restart installation or specify the force flag."
				exit 1
			fi
		fi
		tar -zxvf dahdi-linux-current.tar.gz && rm dahdi-linux-current.tar.gz
		tar -zxvf dahdi-tools-current.tar.gz && rm dahdi-tools-current.tar.gz
		if [ ! -d /usr/src/$DAHDI_LIN_SRC_DIR ]; then
			printf "Directory not found: %s\n" "/usr/src/$DAHDI_LIN_SRC_DIR"
			exit 2
		fi
		cd /usr/src/$DAHDI_LIN_SRC_DIR
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
		if [ ! -d /usr/src/$DAHDI_TOOLS_SRC_DIR ]; then
			printf "Directory not found: %s\n" "/usr/src/$DAHDI_TOOLS_SRC_DIR"
			exit 2
		fi
		cd /usr/src/$DAHDI_TOOLS_SRC_DIR
		dahdi_custom_patch "dahdi_cfg" "$DAHDI_TOOLS_SRC_DIR/dahdi_cfg.c" "https://code.phreaknet.org/asterisk/dahdi/dahdi_cfg.diff" # bug fix for buffer too small for snprintf. See https://issues.asterisk.org/jira/browse/DAHTOOL-89
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
		/sbin/dahdi_genconf modules # in case not in path
		cat /etc/dahdi/modules
		# modprobe <listed>
		/sbin/dahdi_genconf system # in case not in path
		/sbin/dahdi_cfg # in case not in path
	fi
	# Get latest Asterisk LTS version
	cd /usr/src
	rm -f $AST_SOURCE_NAME.tar.gz # the name itself doesn't guarantee that the version is the same
	wget -q --show-progress https://downloads.asterisk.org/pub/telephony/asterisk/$AST_SOURCE_NAME.tar.gz
	if [ $? -ne 0 ]; then
		echoerr "Failed to download file: https://downloads.asterisk.org/pub/telephony/asterisk/$AST_SOURCE_NAME.tar.gz"
		exit 1
	fi
	AST_SRC_DIR=`tar -tzf $AST_SOURCE_NAME.tar.gz | head -1 | cut -f1 -d"/"`
	if [ -d "/usr/src/$AST_SRC_DIR" ]; then
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
	if [ ! -d /usr/src/$AST_SRC_DIR ]; then
		printf "Directory not found: %s\n" "/usr/src/$AST_SRC_DIR"
		exit 2
	fi
	cd /usr/src/$AST_SRC_DIR
	# Install Pre-Reqs
	printf "%s %d" "libvpb1 libvpb1/countrycode string" "$AST_CC" | debconf-set-selections -v
	apt-get install -y libvpb1
	./contrib/scripts/install_prereq install
	./contrib/scripts/get_mp3_source.sh
	./configure --with-jansson-bundled
	cp contrib/scripts/voicemailpwcheck.py /usr/local/bin
	chmod +x /usr/local/bin/voicemailpwcheck.py
	# Change Compile Options: https://wiki.asterisk.org/wiki/display/AST/Using+Menuselect+to+Select+Asterisk+Options
	make menuselect.makeopts
	menuselect/menuselect --enable format_mp3 menuselect.makeopts # add mp3 support
	# We want ulaw, not gsm (default)
	menuselect/menuselect --disable-category MENUSELECT_MOH MENUSELECT_CORE_SOUNDS MENUSELECT_EXTRA_SOUNDS menuselect.makeopts
	# Only grab sounds if this is the first time
	if [ ! -d "$AST_SOUNDS_DIR" ]; then
		# todo: once Pat Fleet prompts are added, have this pull the Pat Fleet prompts
		menuselect/menuselect --enable CORE-SOUNDS-EN-ULAW MOH-OPSOUND-ULAW EXTRA-SOUNDS-EN-ULAW menuselect.makeopts # get the ulaw audio files...
	fi
	if [ "$TEST_SUITE" = "1" ]; then
		menuselect/menuselect --enable DONT_OPTIMIZE BETTER_BACKTRACES TEST_FRAMEWORK menuselect.makeopts
	fi
	if [ "$CHAN_DAHDI" = "1" ]; then
		menuselect/menuselect --enable chan_dahdi menuselect.makeopts
	fi
	if [ "$CHAN_SIP" = "1" ]; then # somebody still wants chan_sip, okay...
		echoerr "chan_sip is deprecated and support will be removed in Asterisk 21. Please move to chan_pjsip at your convenience"
		menuselect/menuselect --enable chan_sip menuselect.makeopts
	else
		menuselect/menuselect --disable chan_sip menuselect.makeopts # remove this in version 21
	fi
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
	# Compile Asterisk
	nice make # compile Asterisk. This is the longest step, if you are installing for the first time. Also, don't let it take over the server.
	# Only generate config if this is the first time
	# Install Asterisk
	if [ ! -d "$AST_CONFIG_DIR" ]; then
		make samples # do not run this on upgrades or it will wipe out your config!
	fi
	rm -f /usr/lib/asterisk/modules/*.so
	make install # actually install modules
	make config # install init script
	make install-logrotate # auto compress and rotate log files

	if [ "$ODBC" = "1" ]; then # MariaDB ODBC for Asterisk
		apt-get install -y unixodbc unixodbc-dev mariadb-server
		cd /usr/src
		wget https://downloads.mariadb.com/Connectors/odbc/connector-odbc-3.1.11/mariadb-connector-odbc-3.1.11-debian-buster-amd64.tar.gz # https://downloads.mariadb.com/Connectors/odbc/connector-odbc-3.1.11/
		tar -zxvf mariadb-connector-odbc-3.1.11-debian-buster-amd64/lib/mariadb
		cd mariadb-connector-odbc-3.1.11-debian-buster-amd64/lib/mariadb
		cp libmaodbc.so /usr/lib/x86_64-linux-gnu/odbc/
		cp libmariadb.so /usr/lib/x86_64-linux-gnu/odbc/
		cd /usr/src
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
		chown -R $AST_USER /etc/asterisk/ /usr/lib/asterisk /var/spool/asterisk/ /var/lib/asterisk/ /var/run/asterisk/ /var/log/asterisk /usr/sbin/asterisk
	fi

	# Development Mode (Asterisk Test Suite)
	if [ "$TEST_SUITE" = "1" ]; then
		install_testsuite
	fi

	/etc/init.d/asterisk status
	/etc/init.d/asterisk start
	/etc/init.d/asterisk status

	printf "%s\n" "Installation has completed. You may now connect to the Asterisk CLI: /sbin/asterisk -r"
	if [ "$CHAN_DAHDI" = "1" ]; then
		echog "Note that DAHDI was installed and requires a reboot before it can be used."
	fi
elif [ "$cmd" = "pulsar" ]; then
	cd /usr/src
	wget https://octothorpe.info/downloads/pulsar-agi.tar.gz
	wget https://code.phreaknet.org/asterisk/pulsar-noanswer.agi # bug fix to pulsar.agi, to fix broken answer supervision:
	mv pulsar-agi.tar.gz /var/lib/asterisk
	cd /var/lib/asterisk
	tar xvfz pulsar-agi.tar.gz # automatically creates /var/lib/asterisk/sounds/pulsar/
	mv /usr/src/pulsar-noanswer.agi /var/lib/asterisk/agi-bin/pulsar.agi
	chmod 755 /var/lib/asterisk/agi-bin/pulsar.agi
	if [ ! -f /var/lib/asterisk/agi-bin/pulsar.agi ]; then
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
elif [ "$cmd" = "installts" ]; then
	if [ "$EUID" -ne 0 ]; then
		echoerr "PhreakScript installts must be run as root. Aborting..."
		exit 2
	fi
	install_testsuite
elif [ "$cmd" = "pubdocs" ]; then
	cd /usr/src
	AST_SRC_DIR=`ls /usr/src | grep "asterisk-" | tail -1`
	apt-get install -y python-dev python-virtualenv python-lxml
	pip install pystache
	pip install premailer
	if [ -d publish-docs ]; then
		rm -rf publish-docs
	fi
	git clone https://github.com/asterisk/publish-docs.git
	cd publish-docs
	printf "%s\n" "Generating Confluence markup..."
	./astxml2wiki.py --username=wikibot --server=https://wiki.asterisk.org/wiki/rpc/xmlrpc '--prefix=Asterisk 18' --space=AST --file=/usr/src/$AST_SRC_DIR/doc/core-en_US.xml --password=d --debug > confluence.txt
	printf "%s\n" "Converting Confluence to HTML..."
	if [ ! -f confluence2html ]; then
		wget https://raw.githubusercontent.com/rmloveland/confluence2html/master/confluence2html
	fi
	chmod +x confluence2html
	PERL_MM_USE_DEFAULT=1 cpan -i CPAN
	perl -MCPAN -e 'install File::Slurp'
	./confluence2html < confluence.txt > docs.html
	ls -l /usr/src/publish-docs
	printf "%s\n" "All Wiki documentation has been generated and is now in /usr/src/publish-docs/docs.html"
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
	# uncommentedlines=`grep "^[^;]" /etc/asterisk/extensions.conf | wc -l`
	# if [ $uncommentedlines -ne 202]; then # the sample config has 202 lines of uncommented config
	# 	# 
	# fi
	if [ ! -d /etc/asterisk/dialplan ]; then
		mkdir -p /etc/asterisk/dialplan
		cd /etc/asterisk/dialplan
	elif [ ! -d /etc/asterisk/dialplan/phreaknet ]; then
		mkdir -p /etc/asterisk/dialplan/phreaknet
		cd /etc/asterisk/dialplan/phreaknet
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
	printf "%s\n" "Backing up /etc/asterisk configs, just in case..."
	mkdir -p /tmp/etc_asterisk
	cp /etc/asterisk/*.conf /tmp/etc/_asterisk # do we really trust users not to make a mistake? backup to tmp, at least...
	printf "%s" "Installing boilerplate code to "
	pwd
	printf "\n"
	wget -q --show-progress https://docs.phreaknet.org/verification.conf -O verification.conf --no-cache
	wget -q --show-progress https://docs.phreaknet.org/phreaknet.conf -O phreaknet.conf --no-cache
	wget -q --show-progress https://docs.phreaknet.org/phreaknet-aux.conf -O phreaknet-aux.conf --no-cache
	ls -l
	cd /etc/asterisk
	wget -q --show-progress https://docs.phreaknet.org/iax.conf -O /etc/asterisk/iax.conf --no-cache
	wget -q --show-progress https://docs.phreaknet.org/sip.conf -O /etc/asterisk/sip.conf --no-cache
	wget -q --show-progress https://docs.phreaknet.org/pjsip.conf -O /etc/asterisk/pjsip.conf --no-cache
	wget -q --show-progress https://docs.phreaknet.org/musiconhold.conf -O /etc/asterisk/musiconhold.conf --no-cache
	wget -q --show-progress https://docs.phreaknet.org/extensions.conf -O /etc/asterisk/extensions.conf --no-cache
	## Inject user config (CLLI code, API key)
	sed -i "s/abcdefghijklmnopqrstuvwxyz/$INTERLINKED_APIKEY/g" /etc/asterisk/extensions.conf
	sed -i "s/WWWWXXYYZZZ/$PHREAKNET_CLLI/g" /etc/asterisk/extensions.conf
	sed -i "s/5551111/$PHREAKNET_DISA/g" /etc/asterisk/extensions.conf
	printf "%s\n" "Updated [globals] in /etc/asterisk/extensions.conf with dynamic variables. If globals are stored in a different file, manual updating is required."
	printf "%s\n" "Boilerplate config installed! Note that these files may still require manual editing before use."
elif [ "$cmd" = "keygen" ]; then
	if [ ${#INTERLINKED_APIKEY} -eq 0 ]; then
		INTERLINKED_APIKEY=`grep -R "interlinkedkey=" /etc/asterisk | grep -v "your-interlinked-api-key" | cut -d'=' -f2 | awk '{print $1;}'`
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
		PHREAKNET_CLLI=`grep -R "clli=" /etc/asterisk | grep -v "5551111" | grep -v "curl " | grep -v "<switch-clli>" | cut -d'=' -f2 | awk '{print $1;}'`
		if [ ${#PHREAKNET_CLLI} -ne 11 ]; then
			echoerr "Failed to find PhreakNet CLLI. For future use, please set your [global] variables, e.g. by running phreaknet config"
			printf '\a'
			read -r -p "PhreakNet CLLI: " PHREAKNET_CLLI
			if [ ${#PHREAKNET_CLLI} -ne 11 ]; then
				echoerr "PhreakNet CLLI must be 11 characters. Not going to attempt uploading the public key..."
			fi
		fi
	fi
	cd /var/lib/asterisk/keys/
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
	touch /etc/asterisk/iax-phreaknet-rsa-in.conf
	touch /etc/asterisk/iax-phreaknet-rsa-out.conf
	## If you are running Asterisk as not root, make the user as which Asterisk runs own the private key and the new files:
	# chown asterisk phreaknetrsa.key
	# chown asterisk /etc/asterisk/iax-phreaknet*
	/sbin/asterisk -rx "module reload res_crypto"
	/sbin/asterisk -rx "keys init"
	/sbin/asterisk -rx "keys show"
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
	if [ ! -d /etc/asterisk/dialplan/phreaknet ]; then
		mkdir -p /etc/asterisk/dialplan/phreaknet
	fi
	touch /etc/asterisk/dialplan/phreaknet/patches.lst
	for file in /tmp/phreakpatch/*; do
		if [ -f $file ]; then
			base=`basename $file`
			if grep -q "$base" /etc/asterisk/dialplan/phreaknet/patches.lst; then
				printf "%s\n" "Patch $base already installed or rejected, not applying again..."
				continue
			fi
			endfile=`printf '%s' $base | cut -d'_' -f2 | cut -d'.' -f1-2`
			if [ -f "/etc/asterisk/dialplan/phreaknet/$endfile" ]; then
				patch -u -b -N /etc/asterisk/dialplan/phreaknet/$endfile -i $file
			elif [ -f "/etc/asterisk/dialplan/phreaknet/$endfile" ]; then
				patch -u -b -N /etc/asterisk/dialplan/phreaknet/$endfile -i $file
			elif [ -f "/etc/asterisk/dialplan/phreaknet/$endfile" ]; then
				patch -u -b -N /etc/asterisk/dialplan/phreaknet/$endfile -i $file
			else
				echoerr "Failed to apply patch $file, couldn't find $endfile!"
				continue
			fi
			echo "$base" >> /etc/asterisk/dialplan/phreaknet/patches.lst
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
elif [ "$cmd" = "upgrade" ]; then
	printf "%s\n" "Updating verification subroutines..."
	if [ -f "/etc/asterisk/verification.conf " ]; then # always update Verification Subroutines
		wget https://docs.phreaknet.org/verification.conf -O /etc/asterisk/verification.conf --no-cache
	elif [ -f "/etc/asterisk/dialplan/phreaknet/verification.conf " ]; then # always update Verification Subroutines
		wget https://docs.phreaknet.org/verification.conf -O /etc/asterisk/dialplan/phreaknet/verification.conf --no-cache
	else
		wget https://docs.phreaknet.org/verification.conf -O /etc/asterisk/dialplan/verification.conf --no-cache
	fi
	# use diff/patch files for the rest, check what version we have and version # of each patch.
	# echo updater
elif [ "$cmd" = "edit" ]; then
	exec nano $FILE_PATH
elif [ "$cmd" = "trace" ]; then
	debugtime=$EPOCHSECONDS
	channel="debug_$debugtime.txt"
	printf "Starting debug: %s\n" "$debugtime"
	/sbin/asterisk -rx "logger add channel $channel notice,warning,error,verbose"
	/sbin/asterisk -rx "core set verbose 10"
	printf "%s\n" "Starting CLI trace..."
	printf '\a'
	read -r -p "A CLI trace is now being collected. Reproduce the issue, then press ENTER to conclude the trace: "
	printf "%s\n" "CLI trace terminated..."
	/sbin/asterisk -rx "logger remove channel $channel"
	if [ ! -f /var/log/asterisk/$channel ]; then
		echoerr "CLI log trace file not found, aborting..."
		exit 2
	fi
	if [ ${#INTERLINKED_APIKEY} -eq 0 ]; then
		INTERLINKED_APIKEY=`grep -R "interlinkedkey=" /etc/asterisk | grep -v "your-interlinked-api-key" | cut -d'=' -f2 | awk '{print $1;}'`
		if [ ${#INTERLINKED_APIKEY} -lt 30 ]; then
			echoerr "Failed to find InterLinked API key. For future use, please set your [global] variables, e.g. by running phreaknet config"
			printf '\a'
			read -r -p "InterLinked API key: " INTERLINKED_APIKEY
			if [ ${#INTERLINKED_APIKEY} -lt 30 ]; then
				echoerr "InterLinked API key too short. Not going to attempt uploading paste..."
				exit 2
			fi
		fi
	fi
	url=`curl -X POST -F "body=@/var/log/asterisk/$channel" -F "key=$INTERLINKED_APIKEY" https://paste.interlinked.us/api/`
	printf "Paste URL: %s\n" "${url}.txt"
	rm /var/log/asterisk/$channel
elif [ "$cmd" = "backtrace" ]; then
	/var/lib/asterisk/scripts/ast_coredumper core --asterisk-bin=/sbin/asterisk
	if [ ! -f /tmp/core-full.txt ]; then
		echoerr "Core dumped failed to get backtrace, aborting..."
		exit 2
	fi
	if [ ${#INTERLINKED_APIKEY} -eq 0 ]; then
		INTERLINKED_APIKEY=`grep -R "interlinkedkey=" /etc/asterisk | grep -v "your-interlinked-api-key" | cut -d'=' -f2 | awk '{print $1;}'`
		if [ ${#INTERLINKED_APIKEY} -lt 30 ]; then
			echoerr "Failed to find InterLinked API key. For future use, please set your [global] variables, e.g. by running phreaknet config"
			printf '\a'
			read -r -p "InterLinked API key: " INTERLINKED_APIKEY
			if [ ${#INTERLINKED_APIKEY} -lt 30 ]; then
				echoerr "InterLinked API key too short. Not going to attempt uploading paste..."
				exit 2
			fi
		fi
	fi
	url=`curl -X POST -F "body=@/tmp/core-full.txt" -F "key=$INTERLINKED_APIKEY" https://paste.interlinked.us/api/`
	printf "Paste URL: %s\n" "${url}.txt"
	rm /tmp/core-*.txt
elif [ "$cmd" = "examples" ]; then
	printf "%s\n\n" 	"========= PhreakScript Example Usages ========="
	printf "%s\n\n" 	"Presented in the logical order of usage, but with multiple variations for each command."
	printf "%s\n\n" 	"phreaknet update                  Update PhreakScript. No Asterisk or configuration modification will occur."
	printf "%s\n\n" 	"Installation commands:"
	printf "%s\n"		"phreaknet install                 Install the latest version of Asterisk."
	printf "%s\n"		"phreaknet install --cc=44         Install the latest version of Asterisk, with country code 44."
	printf "%s\n"		"phreaknet install --force         Reinstall the latest version of Asterisk."
	printf "%s\n"		"phreaknet install --dahdi         Install the latest version of Asterisk, with DAHDI."
	printf "%s\n"		"phreaknet install --sip --weaktls Install Asterisk with chan_sip built AND support for TLS 1.0."
	printf "%s\n"		"phreaknet installts               Install Asterisk Test Suite and Unit Test support (developers only)."
	printf "%s\n"		"phreaknet pulsar                  Install revertive pulsing pulsar AGI, with bug fixes"
	printf "%s\n"		"phreaknet sounds --boilerplate    Install Pat Fleet sounds and basic boilerplate old city tone audio."
	printf "%s\n"		"phreaknet config --force --api-key=<KEY> --clli=<CLLI> --disa=<DISA>"
	printf "%s\n"		"                                  Download and initialize boilerplate PhreakNet configuration"
	printf "%s\n"		"phreaknet keygen                  Upload existing RSA public key to PhreakNet"
	printf "%s\n"		"phreaknet keygen --rotate         Create or rotate PhreakNet RSA keypair, then upload public key to PhreakNet"
	printf "\n%s\n\n"	"Maintenance commands:"
	printf "%s\n"		"phreaknet update                  Update PhreakScript. No Asterisk or configuration modification will occur."
	printf "%s\n"		"phreaknet update --upstream=URL   Update PhreakScript using URL as the upstream source (for testing)."
	printf "%s\n"		"phreaknet patch                   Apply the latest PhreakNet configuration patches."
	printf "\n"
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

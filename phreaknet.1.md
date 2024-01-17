% PHREAKNET(1) PhreakScript 0.1.83
% Naveen Albert
% August 2022

# NAME
phreaknet - install, enhance, configure, and manage Asterisk and DAHDI

# SYNOPSIS
**phreaknet** *command* [*OPTION*]

# DESCRIPTION
**phreaknet** automates the installation, maintenance, and debugging of Asterisk and DAHDI
while integrating additional patches to provide the richest telephony experience. The utility
automatically installs any necessary prerequisites as part of operation.

It also contains tools to automate the process of setting up a new node on PhreakNet
(hence the utility's name). However, it can be used to set up any generic Asterisk system.

PhreakScript installs the latest LTS (Long Term Servicing) version of Asterisk along with
the latest releases of DAHDI Linux and DAHDI Tools (if the -d or --dahdi options are specified).
Many additional bug fixes, features, enhancements, and improvements are also provided to provide
the best Asterisk and DAHDI experience.

# COMMANDS

## Getting started

**about**
: Provides information about PhreakScript

**help**
: Provides a condensed command and option listing

**version**
: Print current PhreakScript version and exit

**examples**
: Print some examples of PhreakScript usage

**info**
: Print current system, Asterisk, and DAHDI version information and exit

**wizard**
: Interactive installation wizard for most common install options

## First Use and Installation

**make**
: Add PhreakScript to path

**man**
: Compile and install PhreakScript man page

**mancached**
: Install cached PhreakScript man page (may be outdated)

**install**
: Install or upgrade Asterisk. This is the primary command provided by PhreakScript.

**source**
: Download and patch source code only, without building or installing. This operates on the current working directory.

**experimental**
: Add experimental features to an existing Asterisk source

**dahdi**
: Install or upgrade DAHDI (only). Generally this command does not need to be used. To install Asterisk with DAHDI, use the install command and provide the -d or --dahdi option instead.

**wanpipe**
: Install or upgrade wanpipe (only). Generally this command does not need to be used. To install Asterisk with DAHDI and wanpipe, use the install command and provide the -d or --dahdi option instead.

**odbc**
: Install ODBC (Open Database Connector) for MariaDB

**installts**
: Install Asterisk Test Suite

**fail2ban**
: Install Asterisk fail2ban configuration

**apiban**
: Install apiban client

**freepbx**
: Install FreePBX GUI (not recommended). This is used to install the FreePBX GUI on an existing Asterisk installation. To install Asterisk with FreePBX, provide the --freepbx flag to the install command instead.

**pulsar**
: Install Revertive Pulsing simulator (audio files and AGI script).

**sounds**
: Install Pat Fleet sound library, overwriting any default Asterisk prompts that they may replace.

**boilerplate-sounds**
: Install PhreakNet boilerplate audio sounds.

**ulaw**
: Convert a wav file to ulaw. If no argument is provided, all wav files in the current directory will be converted. If an argument is provided, only the specified file will be converted.

**remsil**
: Remove silence from one or more WAV audio files. If no argument is provided, all wav files in the current directory will be processed. If an argument is provided, only the specified file will be processed.

**uninstall**
: Uninstall Asterisk, but leave configuration behind

**uninstall-all**
: Uninstall Asterisk, and completely remove all traces of it (configs, etc.)

## Initial Configuration

**bconfig**
: Install PhreakNet boilerplate config

**config**
: Install PhreakNet boilerplate config and autoconfigure PhreakNet variables in the [globals] context in your dialplan

**keygen**
: Install and update PhreakNet RSA keys. If you are installing keys for the first time, you should specify the --rotate flag.

**keyperms**
: Ensure that TLS keys are readable

## Maintenace

**update**
: Update PhreakScript to the latest version. You should run this regularly, and before using this utility for installations. If PhreakScript is out of date, a warning will be displayed before a requested operation is performed.

**patch**
: DEPRECATED. Patch PhreakNet Asterisk configuration.

**genpatch**
: DEPRECATED. Generate a PhreakPatch (patch to be used with the phreaknet patch command)

**alembic**
: Generate an Asterisk Alembic revision. (Developer use only)

**freedisk**
: Free up disk space, useful if disk space is running low. This command will rotate and remove old logs, remove unused swap files, remove old package files, and remove core dump files.

**topdir**
: Show largest directories in current directory

**topdisk**
: Show top files taking up disk space

**enable-swap**
: Temporarily allocate and enable swap file

**disable-swap**
: Disable and deallocate temporary swap file

**restart**
: Fully restart DAHDI and Asterisk

**kill**
: Forcibly kill Asterisk

**forcerestart**
: Forcibly restart Asterisk

**ban**
: Manually ban an IP address using iptables. Argument is the IP address to block.

## Debugging

**dialplanfiles**
: Verify what files are being parsed into the dialplan

**validate**
: DEPRECATED. Run dialplan validation and diagnostics and look for problems

**trace**
: Capture a CLI trace and upload to InterLinked Paste

**paste**
: Upload an arbitrary existing file to InterLinked Paste

**iaxping**
: Check if a remote IAX2 listener is reachable

**pcap**
: Perform a packet capture, optionally against a specific IP address

**pcaps**
: Same as pcap, but open in sngrep afterwards

**sngrep**
: Perform SIP message debugging using **sngrep**

**enable-backtraces**
: Enables backtraces to be extracted from the core dumper (new or existing installs). This may require Asterisk to be recompiled.

**backtrace**
: Use astcoredumper to obtain a backtrace from a core dump and upload to InterLinked Paste

**backtrace-only**
: Use astcoredumper to process a backtrace

**rundump**
: Get a backtrace from the running Asterisk process

**threads**
: Get information about current Asterisk threads

**reftrace**
: Process reference count logs

## Developer Debugging

**valgrind**
: Run Asterisk under valgrind. Asterisk must not be running prior to running this command. Asterisk will be started in the foreground (using the -c console mode).

**cppcheck**
: Run cppcheck on Asterisk for static code analysis

## Development and Testing

**docverify**
: Show documentation validation errors and details

**runtests**
: Run differential PhreakNet tests

**runtest**
: Run a specific PhreakNet test. The argument is the name of the specific test to run.

**stresstest**
: Run any specified test multiple times in a row. The argument is the name of the specific test to run.

**fullpatch**
: Redownload an entire PhreakNet source file from the PhreakScript repository.

**ccache**
: Globally install ccache to speed up recompilation

## Miscellaneous

**docgen**
: DEPRECATED. Generate Asterisk user documentation, using astdocgen.

**mkdocs**
: Generate Asterisk documentation, using Asterisk mkdocs documentation generator.

**pubdocs**
: DEPRECATED. Generate Asterisk user documentation

**applist**
: List Asterisk dialplan applications in current source. This can be useful for seeding text editor syntax files.

**funclist**
: List Asterisk dialplan functions in current source. This can be useful for seeding text editor syntax files.

**edit**
: Edit local PhreakScript source directly

# OPTIONS

**-h**
: Display usage

**-o**, **--flag-test**
: Option flag test. This is a development option only used to verify proper option parsing and handling.

Some options are only used with certain commands.

The following options may be used with the **install** command.

**--audit**
: Audit package installation. At the end of the install, a report will be generated showing what packages were installed.

**-b**, **--backtraces**
: Enables getting backtraces

**-c**, **--cc**
: Country code used for Asterisk installation. Default is 1 (NANPA).

**-d**, **--dahdi**
: Install DAHDI along with Asterisk.

**--devmode**
: Install Asterisk in developer mode. Implicitly true if -t or --testsuite is provided.

**--drivers**
: Also install DAHDI drivers removed in 2018 by Sangoma

**--experimental**
: Install experimental features that may not be production ready

**--extcodecs**
: Specify this if any external codecs are being or will be installed. Failure to do so may result in a crash on startup.

**--fast**
: Compile as fast as possible (recommended for development or idle systems, but not in-place production upgrades)

**-f**, **--force**
: Force install a new version of DAHDI/Asterisk, even if one already exists, overwriting old source
directories if necessary.

**--freepbx**
: Install FreePBX GUI (not recommended)

**--lightweight**
: Only install basic, required modules for basic Asterisk functionality. This may not be suitable for production systems.

**--manselect**
: Manually run menuselect yourself. Generally, this is unnecessary.

**--minimal**
: Do not upgrade the kernel or install nonrequired dependencies (such as utilities that may be useful on typical Asterisk servers)

**-n**, **--no-rc**
: Do not install release candidate versions, if they are available.

**-s**, **--sip**
: Install chan_sip instead of or in addition to chan_pjsip. By default, chan_sip is not compiled or loaded since it is deprecated and will be removed in Asterisk 21.

**--alsa**
: Ensure ALSA library detection exists in the build system. This does NOT readd the deprecated/removed chan_alsa module.

**--cisco**
: Add full support for Cisco Call Manager phones using the usecallmanager patches (chan_sip only)

**--sccp**
: Install community chan_sccp channel driver (Cisco Skinny)

**-t**, **--testsuite**
: Compile with developer support for Asterisk test suite and unit tests.

**-u**, **--user**
: User as which to run Asterisk (non-root). By default, Asterisk is install as root.

**--vanilla**
: Do not install extra features or enhancements. Bug fixes are always installed. (May be required for older versions)

**-v**, **--version**
: Specific version of Asterisk to install (M.m.b e.g. 18.8.0). Also, see **--vanilla**.

The following options may be used with the **sounds** command.

**--boilerplate**
: Also install boilerplate sounds

The following options may be used with the **config** command.

**--api-key**
: InterLinked API key

**--clli**
: CLLI code

The following options may be used with the **keygen** command.

**--rotate**
: Rotate existing RSA keys or create keys if none exist.

The following options may be used with the **update** command.

**--upstream**
: Specify upstream source from which to update PhreakScript. By default, this is the official repository or development mirror.

The following options may be used with the **trace** command.

**--debug**
: Debug level (default is 0/OFF, max is 10)

# EXAMPLES

## Installation and configuration examples

**phreaknet install**
: Install the latest version of Asterisk.

**phreaknet install --cc=44**
: Install the latest version of Asterisk, with country code 44.

**phreaknet install --force**
: Reinstall the latest version of Asterisk.

**phreaknet install --dahdi**
: Install the latest version of Asterisk, with DAHDI.

**phreaknet install --sip --weaktls**
: Install Asterisk with chan_sip built AND support for TLS 1.0.

**phreaknet install --version 18.9.0**
: Install Asterisk version 18.9.0 as the base version of Asterisk.

**phreaknet installts**
: Install Asterisk Test Suite and Unit Test support (developers only)

**phreaknet pulsar**
: Install revertive pulsing pulsar sounds and AGI, with bug fixes

**phreaknet sounds --boilerplate**
: Install Pat Fleet sounds and basic boilerplate old city tone audio

**phreaknet config --force --api-key=<KEY> --clli=<CLLI> --disa=<DISA>**
: Download and initialize boilerplate PhreakNet configuration

**phreaknet keygen**
: Upload existing RSA public key to PhreakNet

**phreaknet keygen --rotate**
: Create or rotate PhreakNet RSA keypair, then upload public key to PhreakNet

**phreaknet validate**
: Validate your dialplan configuration and check for errors

## Debugging examples

**phreaknet trace**
: Perform a trace with verbosity 10 and no debug level (and notify the Business Office)

**phreaknet trace --debug 1**
: Perform a trace with verbosity 10 and debug level 1 (and notify the Business Office)

**phreaknet backtrace**
: Process, extract, and upload a core dump

## Maintenance examples

**phreaknet update**
: Update PhreakScript. No Asterisk or configuration modification will occur.

**phreaknet update --upstream=URL**
: Update PhreakScript using URL as the upstream source (for testing).

**phreaknet patch**
: Apply the latest PhreakNet configuration patches.

**phreaknet fullpatch app_verify**
: Download the latest version of the app_verify module. Recompilation will be required.

# EXIT VALUES

**0**
: Success

**1**
: Error

**2**
: Error

# BUGS

Please report any bugs or issues at https://github.com/InterLinked1/phreakscript

The public mailing list for discussion of this utility may be found at
https://groups.io/g/phreaknet

# COPYRIGHT

Copyright (C) 2022 PhreakNet, Naveen Albert and others.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

Author: AtomicDryad (irc: irc.dashhacks.com #psp-hacks. forums: @qj.net, @psp-hacks.com, @endlessparadigm.com)
Official site: http://pergame.googlecode.com

Desc:
  Pergame is a selective game plugin loader that loads other plugins. The difference from just
    using game.txt is that pergame lets one load plugins for -specific- games, instead of all games.
    Thus one doesn't need to toggle a specific module on or off if they only need it to make one game
    run in m33, and cheat programs that break some homebrew can be told only to load for isos/umds.
    The configuration system aims to be versatile; filenames, partial paths, and umdIDs can be used as
    matching criteria. Also, each iso or eboot may have a configuration file named 'FILENAME.plugins' that
    contains plugins to load (one per line).
  Games that require m33_620.prx/etc run successfully when using this loader instead of
    /seplugins/game.txt.

Changelog:
  0.4: * major fix * iso detection more reliable; using an m33 function now, direct memory reading by address depricated.
         umdID is still detected via memory read until/unless I find a better means.
       * feature * blacklisting modules with '!/seplugins/mod.prx'. Blacklists must be before the entries they blacklist. The top of
         local .plugins conf file is best.
       * fix * psp type and firmware version now detected, and memory addresses are adjusted accordingly.
       * debugging * pergame.txt options added: 'oldisopath=1' (use old memory-read isofinder), umdaddr=0x880137B0 (mem addr where UMDID is found).
0.3.3: * fix * memory string reader now only accepts 7bit strings. iso path finder no longer returns success if it doesn't find
         'ms0:/...'. This should fix problems with pergame assuming the game is an ISO instead of a physical UMD.
       * debugging * isoaddr config entry will force pergame to look at the specified address for the iso's path, for example:
         isoaddr=0x88072408 (no spaces between ...=.. )
0.3.2: * feature * Putting autosort=1 in pergame.txt will cause launched homebrews/iso to move to the top of the xmb menu. 
0.3.1: * fix * Fixed issue with config parser croaking with too many #comments.
  0.3: * feature * now loads GAME.iso.plugins, GAME.plugins, EBOOT.pbp.plugins, EBOOT.plugins and launches module
         entries within.
       * fix * pergame.txt now handles umdID's with dashes, and paths that start with '/'.
       * feature * 'umd' now matches physical UMDs only, 'iso' now matches iso/cso only.
       * fix/debugging * logging function is less crappy
       * debugging * spammy verbose logging with logging=2
  0.2: * fix * UMDID detection now works via direct memory read.
       * feature * relevant modules from 5.03 prometheus, altered to load entirely from MS, are now included.
         default settings in pergame.txt will now cause prometheus to load for any iso beginning with
         'prm' in the following folders: /iso, /iso/cat_game, /iso/cat_mini.
  
Installation/usage:
  Throw the prx anywhere and add to ms0:/seplugins/game.txt. put pergame.txt In the same folder
  and edit. Syntax is (per line):
    module identifier
    'module' is a path to a plugin, like: 'ms0:/path/to/a/plugin.prx'. No spaces. Case insensitive. 'ms0:' not required.
       if module starts with an '!', like '! ms0:/freecheat/fc_3xx.prx', it will never be loaded for matches.
    'identifier' is case insensitve, does not require 'ms0:', can contain spaces, and can be one of the following:
       An ISO like: '/iso/name_of_game.cso'
       An EBOOT like: '/psp/game/filer/eboot.pbp'
       A partial match like: '/psp/game/cat_internet/'
       A UMDID like: 'JPJP65535' or 'usls-99999'
       All physical UMDs: 'umd'
       All ISOs or CSOs: 'iso'
  Examples:
    # comment
    /seplugins/cwcheat/cwcheat.prx	ms0:/iso/cat_game/name_of_game.cso
    ms0:/freecheat/fc_3xx.prx           usls-99999
    !ms0:/seplugins/m33_620.prx		/iso/cat_newgames/crashes-with-620.cso
    /seplugins/m33_620.prx              /iso/cat_newgames/
    /seplugins/lolmodule.prx            /psp/game/cat_testing/lolhomebrew/eboot.pbp
    /seplugins/lolmodule.prx            /psp/game/cat_testing/lolhomebrew/eboo
    See pergame.txt for more examples.
  Also, the module will attempt to load the following files, which can contain one module name per line:
    /iso/name_of_game.iso.plugins, /iso/name_of_game.plugins, /psp/game/name_of_homebrew/eboot.pbp.plugins,
    /psp/game/name_of_homebrew/eboot.plugins
  Examples:
    '/iso/flow.cso.plugins' or '/iso/flow.plugins' for '/iso/flow.cso'
    '/psp/game/filer/eboot.plugins' or '/psp/game/filer/eboot.pbp.plugins' for '/psp/game/filer/eboot.pbp'
  Example contents of /iso/khbbs-jp-eng-patched.plugins:
    ms0:/KHBBS/KHBBS_patch.prx
    !/seplugins/cwcheat/cwcheat.prx
    /LOLpr/LOLprCheatDevice.prx    
  Additional options - case sensitive, no spaces allowed.
    autosort=1 - whenever an eboot is launched, make it move to the top of XMB's game list.

Troubleshooting:
  If something doesn't load, add 'logging=2' to pergame.txt, and post a bugreport to http://code.google.com/p/pergame/issues/list with the logfile.

Debugging:
  You shouldn't need these options unless something doesn't work or needs adjustment for compatibility reasons.
  If you want to troubleshoot yourself, look for 'pergame - isoread' errors in the log and try adding 'oldisopath=1' to pergame.txt. If -that- fails,
  or if there are problems detecting the UMDID, a new NID or new address will need to be located. The addresses can be set without recompilation via
  the 'isoaddr', and 'umdaddr' options. If you locate new values and fix the issue, please post the values to
  http://code.google.com/p/pergame/issues/list so they can be added to a new version.
    logging=1 - log to 'pergame.log'
    logging=2 - log ALOT to 'pergame.log'
    oldisopath=1 - find the ISO filename by direct memory read instead of the m33 function.
    isoaddr=0x........ - the memory address to read. Changes with psp models and FW versions. Not sanity checked.
       Example: 'isoaddr=0x88072408'. Make sure the format is correct as the string isn't sanity checked!.
    offsetiso=0x........ - find the above address via offset to SystemControl's textaddr. Changes with psp models and FW versions.
    umdaddr=0x........ - the memory address to read to find UMDID. Changes with FW versions. Autodetected by default.

Tested with:
  CFW 5.00m33, m33_620.prx, stargate.prx, freecheat, tons of games.

License: 
  GPL. Feel free to distribute, alter, or mutiliate - but you must distribute the source code
    of any derivative works.

Caveats:
  This uses a StartModuleHandler, and I have yet to determine how to turn it off.

Todo:
  Find out why macrofire explodes.
  Ditch the StartModuleHandler.
  DONE: Fix UMD-IDs and real UMDs.
  Fix^H^H^H test np9660 compatibility.
  More power-user/OCD/tweaks such as a better autosorter.
  Maybe? Automatic on-the-fly prometheus patching via temporary SceIO hooks.

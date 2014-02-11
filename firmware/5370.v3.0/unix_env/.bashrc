if [ -f ~/.bashrc.local ]; then
  . ~/.bashrc.local
fi

alias ls="ls --color=auto"
alias la="ls -la"
alias lt="ls -lt"
alias j="jobs -l"
alias mo="more"
alias m="make"
alias mi="make install"
alias mc="make clean"
alias d="dirs"
alias pd="pushd"
alias p2="pushd +2"
alias p3="pushd +3"
alias p4="pushd +4"
alias i="ifconfig -a"
alias df="/bin/df -h"
alias g="function _g () { grep --color=auto -riI \$* . ; } ; _g"
alias g.="function _gdot () { grep --color=auto -iI \$* * ; } ; _gdot"
alias ff="function _ff () { find . -name \$* -print ; } ; _ff"
alias slots="cat /sys/devices/bone_capemgr.*/slots"
alias unslot="echo > /sys/devices/bone_capemgr.*/slots"
alias pins="more /sys/kernel/debug/pinctrl/44e10800.pinmux/pins"
alias pmux="more /sys/kernel/debug/pinctrl/44e10800.pinmux/pinmux-pins"
alias itest="cp /var/lib/cloud9/test_fixture.js /var/lib/cloud9/autorun/test_fixture.js"
alias utest="rm /var/lib/cloud9/autorun/test_fixture.js"
alias h="history"
alias z="suspend"
alias x="exit"
alias tr="traceroute"
alias al="alias"
alias soc="source ~/.profile"
alias pe="printenv"
alias cdp="cd ~/\${PROJ}"
alias dl="curl -o \${PROJ}.tgz www.jks.com/\${PROJ}/\${PROJ}.tgz"
alias xt="cd ~; rm -rf \${PROJ} \${PROJ}.v*; tar xfz \${PROJ}.tgz; rm \${PROJ}.tgz; cdp"
alias clone="git clone git://github.com/jks-prv/5370_proc.git"
alias jc="journalctl"
alias log="journalctl | tail -200"
alias pubip="curl ident.me; echo"
alias sc="systemctl"
alias db="sc reload-or-restart dropbear.socket"

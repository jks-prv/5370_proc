# ~/.profile: executed by Bourne-compatible login shells.

if [ -f ~/.bashrc ]; then
  . ~/.bashrc
fi

# path set by /etc/profile
# export PATH

export PATH=.:$PATH

alias la="ls -la"
alias lt="ls -lt"
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
alias x="exit"
alias al="alias"
alias soc="source ~/.profile"
alias ntp="systemctl reload-or-restart ntpdate"
alias xt="rm -rf \${DIST}; tar xfz \${DIST}.tgz; rm \${DIST}.tgz; cd \${DIST}"
alias log="journalctl | tail -200"
alias pubip="curl ident.me; echo"
alias sc="systemctl"

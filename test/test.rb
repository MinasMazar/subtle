#!/usr/bin/ruby
#
# @package test
#
# @file Run riot unit tests
# @author Christoph Kappel <unexist@subforge.org>
# @version $Id: test/test.rb,v 3131 2011/11/15 20:43:06 unexist $
#
# This program can be distributed under the terms of the GNU GPLv2.
# See the file COPYING for details.
#

# Configuration
subtle   = "../subtle"
subtlext = "../subtlext.so"
config   = "../data/subtle.rb"
sublets  = "./sublet"
display  = ":10"

begin
  require "mkmf"
  require "riot"
  require "gtk2/base"
  require subtlext
rescue LoadError => missing
  puts <<EOF
>>> ERROR: Couldn't find the gem `#{missing}'
>>>        Please install it with following command:
>>>        gem install #{missing}
EOF
end

def fork_and_forget(cmd)
  pid = Process.fork
  if pid.nil?
    exec(cmd)
  else
    Process.detach(pid)
  end
end

# Find xterm
if (xterm = find_executable0("xterm")).nil?
  raise "xterm not found in path"
end

# Start subtle
fork_and_forget("#{subtle} -d #{display} -c #{config} -s #{sublets} &>/dev/null")

sleep 1

# Create dummy tray icon
Gtk.init(["--display=%s" % [ display ]])

Gtk::StatusIcon.new

# Set subtlext display
Subtlext::Subtle.display = display

# Run tests
require_relative "contexts/subtle_init.rb"
require_relative "contexts/color.rb"
require_relative "contexts/geometry.rb"
require_relative "contexts/gravity.rb"
require_relative "contexts/icon.rb"
require_relative "contexts/screen.rb"
require_relative "contexts/sublet.rb"
require_relative "contexts/tag.rb"
require_relative "contexts/view.rb"
require_relative "contexts/client.rb"
require_relative "contexts/tray.rb"
require_relative "contexts/subtle_finish.rb"

# vim:ts=2:bs=2:sw=2:et:fdm=marker

#!/usr/bin/ruby
# -*- encoding: utf-8 -*-
#
# @package subtler
#
# @file Subtle remote
# @copyright (c) 2005-2012 Christoph Kappel <unexist@subforge.org>
# @version $Id: data/subtler/runner.rb,v 3182 2012/02/04 16:39:33 unexist $
#
# This program can be distributed under the terms of the GNU GPLv2.
# See the file COPYING for details.
#

require 'getoptlong'
require 'subtle/subtlext'

module Subtle # {{{
  module Subtler # {{{
    class Runner # {{{

    # Signals
    trap 'INT' do
      exit
    end

      ## initialize {{{
      # Intialize class
      ##

      def initialize
        @mod    = nil
        @group  = nil
        @action = nil
        @proc   = nil

        # Getopt options
        @opts = GetoptLong.new(
          # Groups
          [ '--Client',  '-c', GetoptLong::NO_ARGUMENT ],
          [ '--Gravity', '-g', GetoptLong::NO_ARGUMENT ],
          [ '--Screen',  '-e', GetoptLong::NO_ARGUMENT ],
          [ '--Sublet',  '-s', GetoptLong::NO_ARGUMENT ],
          [ '--Tag',     '-t', GetoptLong::NO_ARGUMENT ],
          [ '--Tray',    '-y', GetoptLong::NO_ARGUMENT ],
          [ '--View',    '-v', GetoptLong::NO_ARGUMENT ],

          # Actions
          [ '--add',     '-a', GetoptLong::NO_ARGUMENT ],
          [ '--kill',    '-k', GetoptLong::NO_ARGUMENT ],
          [ '--find',    '-f', GetoptLong::NO_ARGUMENT ],
          [ '--focus',   '-o', GetoptLong::NO_ARGUMENT ],
          [ '--full',    '-F', GetoptLong::NO_ARGUMENT ],
          [ '--float',   '-O', GetoptLong::NO_ARGUMENT ],
          [ '--stick',   '-S', GetoptLong::NO_ARGUMENT ],
          [ '--urgent',  '-N', GetoptLong::NO_ARGUMENT ],
          [ '--jump',    '-j', GetoptLong::NO_ARGUMENT ],
          [ '--list',    '-l', GetoptLong::NO_ARGUMENT ],
          [ '--tag',     '-T', GetoptLong::NO_ARGUMENT ],
          [ '--untag',   '-U', GetoptLong::NO_ARGUMENT ],
          [ '--tags',    '-G', GetoptLong::NO_ARGUMENT ],
          [ '--retag',   '-A', GetoptLong::NO_ARGUMENT ],
          [ '--clients', '-I', GetoptLong::NO_ARGUMENT ],
          [ '--views',   '-W', GetoptLong::NO_ARGUMENT ],
          [ '--update',  '-u', GetoptLong::NO_ARGUMENT ],
          [ '--data',    '-D', GetoptLong::NO_ARGUMENT ],
          [ '--gravity', '-Y', GetoptLong::NO_ARGUMENT ],
          [ '--screen',  '-n', GetoptLong::NO_ARGUMENT ],
          [ '--raise',   '-E', GetoptLong::NO_ARGUMENT ],
          [ '--lower',   '-L', GetoptLong::NO_ARGUMENT ],

          # Modifiers
          [ '--reload',  '-r', GetoptLong::NO_ARGUMENT       ],
          [ '--restart', '-R', GetoptLong::NO_ARGUMENT       ],
          [ '--quit',    '-q', GetoptLong::NO_ARGUMENT       ],
          [ '--current', '-C', GetoptLong::NO_ARGUMENT       ],
          [ '--select',  '-X', GetoptLong::NO_ARGUMENT       ],
          [ '--proc',    '-p', GetoptLong::REQUIRED_ARGUMENT ],

          # Other
          [ '--display', '-d', GetoptLong::REQUIRED_ARGUMENT ],
          [ '--help',    '-h', GetoptLong::NO_ARGUMENT       ],
          [ '--version', '-V', GetoptLong::NO_ARGUMENT       ]
        )
      end # }}}

      ## run {{{
      # Run subtler
      ##

      def run
        # Parse arguments
        @opts.each do |opt, arg|
          case opt
            # Groups
            when '--Client'  then @group  = Subtlext::Client
            when '--Gravity' then @group  = Subtlext::Gravity
            when '--Screen'  then @group  = Subtlext::Screen
            when '--Sublet'  then @group  = Subtlext::Sublet
            when '--Tag'     then @group  = Subtlext::Tag
            when '--Tray'    then @group  = Subtlext::Tray
            when '--View'    then @group  = Subtlext::View

            # Actions
            when '--add'     then @action = :new
            when '--kill'    then @action = :kill
            when '--find'    then @action = :find
            when '--focus'   then @action = :focus
            when '--full'    then @action = :toggle_full
            when '--float'   then @action = :toggle_float
            when '--stick'   then @action = :toggle_stick
            when '--urgent'  then @action = :toggle_urgent
            when '--jump'    then @action = :jump
            when '--list'    then @action = :list
            when '--tag'     then @action = :tag
            when '--untag'   then @action = :untag
            when '--tags'    then @action = :tags
            when '--retag'   then @action = :retag
            when '--clients' then @action = :clients
            when '--views'   then @action = :views
            when '--update'  then @action = :update
            when '--data'    then @action = :send_data
            when '--gravity' then @action = :gravity=
            when '--raise'   then @action = :raise
            when '--lower'   then @action = :lower

            # Modifiers
            when '--reload'  then @mod = :reload
            when '--restart' then @mod = :restart
            when '--quit'    then @mod = :quit
            when '--current' then @mod = :current
            when '--select'  then @mod = :select

            # Other
            when '--proc'
              @proc = Proc.new { |param| eval(arg) }
            when '--display'
              Subtlext::Subtle.display = arg 
            when '--help'
              usage(@group)
              exit
            when '--version'
              version
              exit
            else
              usage
              exit
          end
        end

        # Get arguments
        arg1 = ARGV.shift
        arg2 = ARGV.shift

        # Convert window ids
        arg1 = Integer(arg1) rescue arg1
        arg2 = Integer(arg2) rescue arg2

        # Pipes?
        arg1 = ARGF.read.chop if '-' == arg1

        if '-' == arg2
          # Read pipe until EOF
          begin
            while (arg2 = ARGF.readline) do
              handle_command(arg1, arg2.chop)
            end
          rescue EOFError
            # Just ignore this
          end
        else
          handle_command(arg1, arg2)
        end
      end # }}}

      private

      def handle_command(arg1, arg2) # {{{
        # Modifiers
        case @mod
          when :reload  then  Subtlext::Subtle.reload
          when :restart then  Subtlext::Subtle.restart
          when :quit    then  Subtlext::Subtle.quit
          when :current
            arg2 = arg1
            arg1 = :current
          when :select
            arg2 = arg1
            arg1 = Subtlext::Subtle.select_window
        end

        # Call method
        begin
          if !@group.nil? and !@action.nil?
            # Check singleton and instance methods
            if (@group.singleton_methods << :new).include?(@action)
              obj   = @group
              args  = [ arg1, arg2 ].compact
              arity = obj.method(@action).arity
            elsif @group.instance_methods.include?(@action)
              obj   = @group.send(:find, arg1)
              args  = [ arg2 ]
              arity = @group.instance_method(@action).arity
            end

            # Handle different arities
            case arity
              when 1
                # Handle different return types
                if obj.is_a?(Array)
                  obj.each do |o|
                    p '%s:' % o if 1 < obj.size
                    handle_result(o.send(@action, *args))
                  end
                else
                  handle_result(obj.send(@action, *args))
                end
              when -1
                if [ Subtlext::Sublet, Subtlext::Tag,
                    Subtlext::View, Subtlext::Gravity ].include?(@group)
                  # Create new object
                  ret = obj.send(@action, *args)
                  ret.save

                  handle_result(ret)
                end

                exit
              when nil
                usage(@group)
                exit
              else
                # Handle different return types
                if obj.is_a?(Array)
                  obj.each do |o|
                    p '%s:' % o if 1 < obj.size
                    handle_result(o.send(@action))
                  end
                else
                  handle_result(obj.send(@action))
                end
            end
          elsif :reload != @mod and :restart != @mod and :quit != @mod
            usage(@group)
            exit
          end
        rescue ArgumentError
          usage(@group)
          exit
        end
      end # }}}

      def call_or_print(value) # {{{
        unless @proc.nil?
          @proc.call(value)
        else
          printer(value)
        end
      end # }}}

      def handle_result(result) # {{{
        case result
          when Array
            result.each do |r|
              call_or_print(r)
            end
          else
            call_or_print(result)
        end
      end # }}}

      def printer(value) # {{{
        case value
          when Subtlext::Client # {{{
            puts '%#10x %s %d %4d x %4d + %4d + %-4d %12.12s %s%s%s%s%s%s %s (%s)' % [
              value.win,
              value.views.map { |v| v.name }.include?(Subtlext::View.current.name) ? '*' : '-',
              Subtlext::View.current.id,
              value.geometry.x,
              value.geometry.y,
              value.geometry.width,
              value.geometry.height,
              value.gravity.name,
              (value.is_full?)   ? '+' : '-',
              (value.is_float?)  ? '^' : '-',
              (value.is_stick?)  ? '*' : '-',
              (value.is_resize?) ? '~' : '-',
              (value.is_zaphod?) ? '=' : '-',
              (value.is_fixed?)  ? '!' : '-',
              value.instance,
              value.klass
            ] # }}}
          when Subtlext::Gravity # {{{
            puts '%15s %3d x %-3d %3d + %-3d' % [
              value.name,
              value.geometry.x,
              value.geometry.y,
              value.geometry.width,
              value.geometry.height,
            ] # }}}
          when Subtlext::Screen # {{{
            puts '%d %4d x %-4d %4d + %-4d' % [
              value.id,
              value.geometry.x,
              value.geometry.y,
              value.geometry.width,
              value.geometry.height,
            ] # }}}
          when Subtlext::Tag # {{{
            puts value.name # }}}
          when Subtlext::Tray # {{{
            puts '%#10x %s (%s)' % [
              value.win,
              value.instance,
              value.klass
            ] # }}}
          when Subtlext::Sublet # {{{
            puts '%s' % [ value.name ] # }}}
          when Subtlext::View # {{{
            puts '%2d %s %s' % [
              value.id,
              Subtlext::View.visible.include?(value) ? '*' : '-',
              value.name
            ] # }}}
        end
      end # }}}

      def usage(group) # {{{
        puts 'Usage: subtler [GENERIC|MODIFIER] GROUP ACTION [ARG1] [ARG2]\n\n'

        if group.nil?
          puts <<-EOF
  Generic:
    -d, --display=DISPLAY  Connect to DISPLAY (default: #{ENV['DISPLAY']})
    -h, --help             Show this help and exit
    -V, --version          Show version info and exit

  Modifier:
    -r, --reload           Reload config and sublets
    -R, --restart          Restart subtle
    -q, --quit             Quit subtle
    -C, --current          Select current active window/view
                           instead of passing it via argument
    -X, --select           Select a window via pointer instead
                           of passing it via argument

  Groups:
    -c, --Client           Use client group
    -g, --Gravity          Use gravity group
    -e, --Screen           Use screen group
    -s, --Sublet           Use sublet group
    -t, --Tag              Use tag group
    -y, --Tray             Use tray group
    -v, --View             Use views group

          EOF
        end

        if group.nil? or Subtlext::Client == group
          puts <<-EOF
  Actions for clients (-c, --Client):
    -f, --find    => -cf PATTERN            Find client
    -o, --focus   => -co PATTERN            Set focus to client
    -F, --full    => -cF PATTERN            Toggle full
    -O, --float   => -cO PATTERN            Toggle float
    -S, --stick   => -cS PATTERN            Toggle stick
    -N, --urgent  => -cN PATTERN            Toggle urgent
    -l, --list                              List all clients
    -T, --tag     => -c PATTERN -T NAME     Add tag to client
    -U, --untag   => -c PATTERN NAME        Remove tag from client
    -G, --tags    => -cG PATTERN            Show client tags
    -Y, --gravity => -c PATTERN -Y PATTERN  Set client gravity
    -E, --raise   => -cE PATTERN            Raise client window
    -L, --lower   => -cL PATTERN            Lower client window
    -k, --kill    => -ck PATTERN            Kill client

          EOF
        end

        if group.nil? or Subtlext::Gravity == group
          puts <<-EOF
  Actions for gravities (-g, --Gravity):
    -a, --add     => -g NAME -a GEOMETRY    Create new gravity
    -l, --list                              List all gravities
    -f, --find    => -gf PATTERN            Find a gravity
    -k, --kill    => -gk PATTERN            Kill gravity

          EOF
        end

        if group.nil? or Subtlext::Screen == group
          puts <<-EOF
  Actions for screens (-e, --Screen):
    -l, --list                              List all screens
    -f, --find    => -ef ID                 Find a screen

          EOF
        end

        if group.nil? or Subtlext::Sublet == group
          puts <<-EOF
  Actions for sublets (-s, --Sublet):
    -l, --list                              List all sublets
    -f, --find    => -sf PATTERN            Find sublet
    -u, --update  => -su PATTERN            Updates value of sublet
    -D, --data    => -s PATTERN -D DATA     Send data to sublet
    -k, --kill    => -sk PATTERN            Kill sublet

          EOF
        end

        if group.nil? or Subtlext::Tag == group
          puts <<-EOF
  Actions for tags (-t, --Tag):
    -a, --add     => -ta NAME               Create new tag
    -f, --find    => -tf PATTERN            Find all clients/views by tag
    -l, --list                              List all tags
    -I, --clients => -tI PATTERN            Show clients with tag
    -k, --kill    => -tk PATTERN            Kill tag

          EOF
        end

        if group.nil? or Subtlext::Tray == group
          puts <<-EOF
  Actions for tray (-y, --Tray):
    -f, --find    => -yf PATTERN            Find all tray icons
    -l, --list                              List all tray icons
    -k, --kill    => -yk PATTERN            Kill tray icon

          EOF
        end

        if group.nil? or Subtlext::View == group
          puts <<-EOF
  Actions for views (-v, --View):
    -a, --add     => -va NAME               Create new view
    -f, --find    => -vf PATTERN            Find a view
    -l, --list                              List all views
    -T, --tag     => -v PATTERN -T NAME     Add tag to view
    -U, --untag   => -v PATTERN -U NAME     Remove tag from view
    -G, --tags                              Show view tags
    -I, --clients                           Show clients on view
    -k, --kill    => -vk PATTERN            Kill view

          EOF
        end

        puts <<-EOF
  Formats:

  Input:
    DISPLAY:  :<display number>
    ID:       <number>
    GEOMETRY: <x>x<y>+<width>+<height>
    NAME:     <string|number>
    DATA      <string|number>
    PATTERN:
      Matching works either via plaintext, regex (see regex(7)), id or window id
      if applicable. If a pattern matches more than once ALL matches are used.

      If the PATTERN is '-' subtler will read from stdin.

  Output:
    Client listing:  <window id> <visibility> <view id> <geometry> <gravity> <flags> <instance name> (<class name>)
    Gravity listing: <gravity id> <geometry>
    Screen listing:  <screen id> <geometry>
    Tag listing:     <tag name>
    Tray listing:    <window id> <instance name> (<class name>)
    View listing:    <window id> <visibility> <view id> <view name>

  Fields:
    <window id>      Numeric (hex) id of window (e.g. 0xa00009)
    <visibility>     - = not visible, * = visible
    <view id>        Numeric id of view (e.g. 5)
    <geometry>       x x y + width + height
    <flags>          - = not set, + = fullscreen, ^ = float, * = stick, ~ = resize, = = zaphod, ! = fixed
    <instance name>  Window instance/resource name
    <class name>     Window class name
    <gravity id>     Numeric id of gravity (e.g. 2)
    <screen id>      Numeric id of a screen (e.g. 1)
    <tag name>       Name of a tag (e.g. terms)

  Examples:
    subtler -c -l               List all clients
    subtler -t -a subtle        Add new tag 'subtle'
    subtler -v subtle -T rocks  Tag view 'subtle' with tag 'rocks'
    subtler -c xterm -G         Show tags of client 'xterm'
    subtler -c -X -f            Select client and show info
    subtler -c -C -Y 5          Set gravity 5 to current active client
    subtler -t -f term          Show every client/view tagged with 'term'

  Please report bugs at http://subforge.org/projects/subtle/issues

        EOF
      end # }}}

      def version # {{{
        puts <<-EOF
  subtler #{Subtlext::VERSION} - Copyright (c) 2005-2012 Christoph Kappel
  Released under the GNU General Public License
  Using Ruby #{RUBY_VERSION}
        EOF
      end # }}}
    end # }}}
  end # }}}
end # }}}

# vim:ts=2:bs=2:sw=2:et:fdm=marker

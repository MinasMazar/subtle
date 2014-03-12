# -*- encoding: utf-8 -*-
#
# @package sur
#
# @file Runner functions
# @author Christoph Kappel <unexist@subforge.org>
# @version $Id: data/sur/runner.rb,v 3182 2012/02/04 16:39:33 unexist $
#
# This program can be distributed under the terms of the GNU GPLv2.
# See the file COPYING for details.
#

require "subtle/sur/client"

module Subtle # {{{
  module Sur # {{{
    # Runner class for argument dispatching
    class Runner # {{{
      # Trap SIGINT
      trap "INT" do
        exit
      end

      ## Subtle::Sur::Runner::run {{{
      # Start runner
      #
      # @param [Array]  args  Argument array
      #
      # @raise [String] Command error
      # @since 0.0
      #
      #  Subtle::Sur::Runner.new.run(args)
      #  => nil

      def run(args)
        # Default options
        @repo      = "local"
        @version   = nil
        @port      = 4567
        @use_tags  = false
        @use_color = true
        @use_regex = false
        @reload    = false
        @assume    = false
        @config    = []

        # GetoptLong only works on ARGV so we use something own
        begin
          idx    = 0
          no_opt = []

          while(idx < args.size)
            case args[idx]
              # Enable flags
              when "-l", "--local"    then @repo      = "local"
              when "-r", "--remote"   then @repo      = "remote"
              when "-c", "--no-color" then @use_color = false
              when "-e", "--regex"    then @use_regex = true
              when "-t", "--tag"      then @use_tags  = true
              when "-R", "--reload"   then @reload    = true
              when "-y", "--yes"      then @assume    = true

              # Get command help
              when "-h", "--help"
                usage(args[0])
                exit

              # Get option arguments
              when "-v", "--version"
                idx      += 1
                @version  = args[idx]
              when "-p", "--port"
                idx   += 1
                @port  = args[idx].to_i
              when "-C", "--config"
                idx     += 1
                @config << args[idx]

              # Save other arguments
              else
                no_opt << args[idx]
            end

            idx += 1
          end

          args = no_opt
        rescue
          raise "Cannot find required arguments"
        end

        # Run
        cmd  = args.shift
        case cmd
          when "annotate"
            usage(cmd) if args.empty?

            Subtle::Sur::Client.new.annotate(args.first, @version)
          when "build"
            usage(cmd) if args.empty?

            Subtle::Sur::Client.new.build(args.first)
          when "config"
            usage(cmd) if args.empty?

            Subtle::Sur::Client.new.config(args.first, @use_color)
          when "fetch"
            usage(cmd) if args.empty?

            Subtle::Sur::Client.new.fetch(args, @version, @use_tags)
          when "grabs"
            usage(cmd) if args.empty?

            Subtle::Sur::Client.new.grabs(args.first, @use_color)
          when "help" then usage(nil)
          when "info"
            usage(cmd) if args.empty?

            Sur::Client.new.info(args, @use_color)
          when "install"
            usage(cmd) if args.empty?

            Sur::Client.new.install(args, @version, @use_tags, @reload)
          when "list"
            Subtle::Sur::Client.new.list(@repo, @use_color)
          when "notes"
            usage(cmd) if args.empty?

            Subtle::Sur::Client.new.notes(args.first)
          when "query"
            usage(cmd) if args.empty?

            Subtle::Sur::Client.new.query(args.first, @repo,
              @version, @use_regex, @use_tags, @use_color
            )
          when "reorder"
            Subtle::Sur::Client.new.reorder
          when "server"
            require "subtle/sur/server"

            Sur::Server.new(@port).run
          when "submit"
            usage(cmd) if args.empty?

            Subtle::Sur::Client.new.submit(args.first)
          when "template"
            usage(cmd) if args.empty?

            Subtle::Sur::Specification.template(args.first)
          when "test"
            usage(cmd) if args.empty?

            require "subtle/sur/test"

            Subtle::Sur::Test.run(@config, args)
          when "uninstall"
            usage(cmd) if args.empty?

            Subtle::Sur::Client.new.uninstall(args,
              @version, @use_tags, @reload)
          when "unpack"
            Subtle::Sur::Client.new.unpack(args,
              @version, @use_tags)
          when "update"
            Subtle::Sur::Client.new.update(@repo)
          when "upgrade"
            Subtle::Sur::Client.new.upgrade(@use_color, @assume, @reload)
          when "version" then version
          when "yank"
            usage(cmd) if args.empty?

            Subtle::Sur::Client.new.update(args.first, @version)
          else usage(nil)
        end
      end # }}}

      private

      def usage(arg) # {{{
        case arg
          when "annotate"
            puts "Usage: sur annotate NAME [OPTIONS]\n\n" \
                 "Mark sublet as to be reviewed\n\n" \
                 "Options:\n" \
                 "  -v, --version VERSION     Annotate a specific version\n" \
                 "  -h, --help                Show this help and exit\n\n" \
                 "Examples:\n" \
                 "sur annotate -l\n" \
                 "sur annotate -r\n"
          when "fetch"
            puts "Usage: sur fetch NAME [OPTIONS]\n\n" \
                 "Download sublet to current directory\n\n" \
                 "Options:\n" \
                 "  -t, --tags                Include tags in search\n" \
                 "  -v, --version VERSION     Search for a specific version\n" \
                 "  -h, --help                Show this help and exit\n\n" \
                 "Examples:\n" \
                 "sur fetch clock\n" \
                 "sur fetch clock -v 0.3\n"
          when "install"
            puts "Usage: sur install NAME [OPTIONS]\n\n" \
                 "Install a sublet by given name\n\n" \
                 "Options:\n" \
                 "  -R, --reload              Reload sublets after install\n" \
                 "  -t, --tags                Include tags in search\n" \
                 "  -v, --version VERSION     Search for a specific version\n" \
                 "  -h, --help                Show this help and exit\n\n" \
                 "Examples:\n" \
                 "sur install clock\n" \
                 "sur install clock -v 0.3\n"
          when "list"
            puts "Usage: sur list [OPTIONS]\n\n" \
                 "List sublets in repository\n\n" \
                 "Options:\n" \
                 "  -l, --local               Select local repository (default)\n" \
                 "  -r, --remote              Select remote repository\n" \
                 "  -h, --help                Show this help and exit\n\n" \
                 "Examples:\n" \
                 "sur list -l\n" \
                 "sur list -r\n"
          when "query"
            puts "Usage: sur query NAME [OPTIONS]\n\n" \
                 "Query a repository for a sublet by given name\n\n" \
                 "Options:\n" \
                 "  -e, --regex               Use regex for query\n" \
                 "  -l, --local               Select local repository (default)\n" \
                 "  -r, --remote              Select remote repository\n" \
                 "  -t, --tags                Include tags in search\n" \
                 "  -v, --version VERSION     Search for a specific version\n" \
                 "  -h, --help                Show this help and exit\n\n" \
                 "Examples:\n" \
                 "sur query -l clock\n" \
                 "sur query -r clock -v 0.3\n"
          when "server"
            puts "Usage: sur server [OPTIONS]\n\n" \
                 "Serve sublets on localhost and optionally on a given port\n\n" \
                 "Options:\n" \
                 "  -p, --port PORT           Select a specific port\n" \
                 "  -h, --help                Show this help and exit\n\n" \
                 "Examples:\n" \
                 "sur server -p 3000\n"
          when "test"
            puts "Usage: sur test NAME [OPTIONS]\n\n" \
                 "Test given sublets for syntax and functionality\n\n" \
                 "Options:\n" \
                 "  -C, --config VALUE        Add config value (can be used multiple times)\n" \
                 "  -h, --help                Show this help and exit\n\n" \
                 "Examples:\n" \
                 "sur test -C user=name -C pass=pass sublet.rb\n" \
                 "sur test sublet.rb\n"
          when "unpack"
            puts "Usage: sur unpack NAME [OPTIONS]\n\n" \
                 "Unpack sublet to current path\n\n" \
                 "Options:\n" \
                 "  -t, --tags                Include tags in search\n" \
                 "  -v, --version VERSION     Search for a specific version\n" \
                 "  -h, --help                Show this help and exit\n\n" \
                 "Examples:\n" \
                 "sur unpack clock\n" \
                 "sur unpack clock -v 0.3\n"
          when "uninstall"
            puts "Usage: sur uninstall NAME [OPTIONS]\n\n" \
                 "Uninstall a sublet by given name and optionally by given version\n\n" \
                 "Options:\n" \
                 "  -R, --reload              Reload sublets after uninstall\n" \
                 "  -t, --tags                Include tags in search\n" \
                 "  -v, --version VERSION     Select a specific version\n" \
                 "  -h, --help                Show this help and exit\n\n" \
                 "Examples:\n" \
                 "sur uninstall clock\n" \
                 "sur uninstall clock -v 0.3\n"
          when "update"
            puts "Usage: sur update [-l|-r|-h]\n\n" \
                 "Update local/remote sublet cache\n\n" \
                 "Options:\n" \
                 "  -l, --local               Select local repository (default)\n" \
                 "  -r, --remote              Select remote repository\n" \
                 "  -h, --help                Show this help and exit\n\n" \
                 "Examples:\n" \
                 "sur update -l\n" \
                 "sur update -r\n"
          when "upgrade"
            puts "Usage: sur upgrade [-R|-h]\n\n" \
                 "Upgrade all sublets if possible\n\n" \
                 "Options:\n" \
                 "  -R, --reload              Reload sublets after upgrading\n" \
                 "  -y, --yes                 Assume yes to questions\n" \
                 "  -h, --help                Show this help and exit\n\n" \
                 "Examples:\n" \
                 "sur upgrade\n" \
                 "sur upgrade -R\n"
          else
            puts "Usage: sur [OPTIONS]\n\n" \
                 "Options:\n" \
                 "  annotate NAME [-v VERSION|-h]           Mark sublet to be reviewed\n" \
                 "  build SPEC                              Create a sublet package\n" \
                 "  config NAME                             Show available config settings of a sublet\n" \
                 "  fetch NAME                              Download sublet to current directory\n" \
                 "  help                                    Show this help and exit\n" \
                 "  grabs NAME                              Show available grabs provided by a sublet\n" \
                 "  info NAME                               Show info about an installed sublet\n" \
                 "  install NAME [-R|-t|-v VERSION|-h]      Install a sublet\n" \
                 "  list [-l|-r|-h]                         List local/remote sublets\n" \
                 "  notes NAME                              Show notes about a sublet\n" \
                 "  query NAME [-e|-l|-t|-v VERSION|-h]     Query for a sublet (e.g clock, clock -v 0.3)\n" \
                 "  reorder                                 Reorder installed sublets for loading order\n" \
                 "  server [-p PORT|-h]                     Serve sublets (default: http://localhost:4567)\n" \
                 "  submit FILE                             Submit a sublet to SUR\n" \
                 "  template FILE                           Create a new sublet template in current dir\n" \
                 "  test NAME [-C VALUE|-h]                 Test sublets for syntax and functionality\n" \
                 "  uninstall NAME [-R|-t|-v VERSION|-h]    Uninstall a sublet\n" \
                 "  unpack NAME [-t|-v VERSION|-h]          Unpack a sublet in current directory\n" \
                 "  update [-l|-r|-h]                       Update local/remote sublet cache\n" \
                 "  upgrade [-R|-y|-h]                      Upgrade all installed sublets\n" \
                 "  version                                 Show version info and exit\n"
                 "  yank [-v VERSION]                       Delete sublet from server\n"
        end

        puts "\nPlease report bugs at http://subforge.org/projects/subtle/issues\n"

        exit
      end # }}}

      def version # {{{
        puts "Sur #{VERSION} - Copyright (c) 2009-2012 Christoph Kappel\n" \
             "Released under the GNU General Public License\n"
      end # }}}
    end # }}}
  end # }}}
end # }}}

# vim:ts=2:bs=2:sw=2:et:fdm=marker

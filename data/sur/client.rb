# -*- encoding: utf-8 -*-
#
# @package sur
#
# @file Client functions
# @author Christoph Kappel <unexist@subforge.org>
# @version $Id: data/sur/client.rb,v 3182 2012/02/04 16:39:33 unexist $
#
# This program can be distributed under the terms of the GNU GPLv2.
# See the file COPYING for details.
#

require "rubygems"
require "fileutils"
require "tempfile"
require "yaml"
require "uri"
require "net/http"
require "archive/tar/minitar"

require "subtle/sur/specification"

module Subtle # {{{
  module Sur # {{{
    # Client class for interaction with the user
    class Client # {{{
      # Remote repository host
      HOST = "http://sur.subforge.org"

      # Header separator
      BOUNDARY = "AaB03x"

      # Local sublet cache
      attr_accessor :cache_local

      # Remote sublet cache
      attr_accessor :cache_remote

      # Path to local cache file
      attr_accessor :path_local

      # Path to remote cache file
      attr_accessor :path_remote

      # Path to icons
      attr_accessor :path_icons

      # Path of specification
      attr_accessor :path_specs

      # Path to sublets
      attr_accessor :path_sublets

      ## Sur::Client::initialize {{{
      # Create a new Client object
      #
      # @return [Object] New Client
      #
      # @since 0.0
      #
      # @example
      #   client = Sur::Client.new
      #   => #<Sur::Client:xxx>

      def initialize
        # Paths
        xdg_cache_home = File.join((ENV["XDG_CACHE_HOME"] ||
          File.join(ENV["HOME"], ".cache")), "sur")
        xdg_data_home  = File.join((ENV["XDG_DATA_HOME"] ||
          File.join(ENV["HOME"], ".local", "share")), "subtle")

        @path_local   = File.join(xdg_cache_home, "local.yaml")
        @path_remote  = File.join(xdg_cache_home, "remote.yaml")
        @path_icons   = File.join(xdg_data_home,  "icons")
        @path_specs   = File.join(xdg_data_home,  "specifications")
        @path_sublets = File.join(xdg_data_home,  "sublets")

        # Create folders
        [ xdg_cache_home, xdg_data_home, @path_icons,
            @path_specs, @path_sublets ].each do |p|
          FileUtils.mkdir_p([ p ]) unless File.exist?(p)
        end
      end # }}}

      ## Sur::Client::annotate {{{
      # Mark a sublet as to be reviewed
      #
      # @param [String]  name     Name of the Sublet
      # @param [String]  version  Version of the Sublet
      #
      # @raise [String] Annotate error
      # @since 0.2
      #
      # @example
      #   Sur::Client.new.submit("sublet")
      #   => nil

      def annotate(name, version = nil)
        build_local
        build_remote

        # Check if sublet exists
        if (specs = search(name, @cache_remote, version, false)) and specs.empty?
          raise "Sublet `#{name}' does not exist"
        end

        spec = specs.first
        uri  = URI.parse(HOST + "/annotate")
        res  = Net::HTTP.post_form(uri,
          {
            "digest" => spec.digest,
            "user"   => ENV["USER"]
          }
        )

        raise "Cannot annotate sublet: Sublet not found" if 404 == res.code
      end # }}}

      ## Sur::Client::build {{{
      # Build a sublet package
      #
      # @param [String]  file  Spec file name
      #
      # @see Sur::Specification.load_file
      # @raise [String] Build error
      # @since 0.1
      #
      # @example
      #   Sur::Client.new.build("sublet.spec")
      #   => nil

      def build(file)
        spec = Sur::Specification.load_spec(file)

        # Check specification
        if spec.valid?
          begin
            sublet = "%s-%s.sublet" % [
              File.join(Dir.pwd, spec.name.downcase),
              spec.version
            ]
            opts = { :mode => 644, :mtime => Time.now.to_i }

            # Check if files exist
            (spec.files | spec.icons).each do |f|
              unless File.exist?(File.join(File.dirname(file), f))
                raise "Cannot find file `#{f}'"
              end
            end

            # Check gem version
            unless spec.dependencies.empty?
              spec.dependencies.each do |name, version|
                if version.match(/^(\d*\.){1,2}\d*$/)
                  puts ">>> WARNING: Gem dependency `%s' " \
                       "requires exact gem version (=%s)" % [ name, version ]
                end
              end
            end

            # Create tar file
            File.open(sublet, "wb") do |output|
              Archive::Tar::Minitar::Writer.open(output) do |tar|
                # Add Spec
                tar.add_file(File.basename(file), opts) do |os|
                  os.write(IO.read(file))
                end

                # Add files
                spec.files.each do |f|
                  tar.add_file(File.basename(f), opts) do |os|
                    os.write(IO.read(File.join(File.dirname(file), f)))
                  end
                end

                # Add icons
                spec.icons.each do |f|
                  tar.add_file(File.basename(f), opts) do |os|
                    os.write(IO.read(File.join(File.dirname(file), f)))
                  end
                end
              end
            end

            puts ">>> Created sublet `#{File.basename(sublet)}'"
          rescue => error
            raise error.to_s
          end
        else
          spec.validate
        end
      end # }}}

      ## Sur::Client::config {{{
      # Show config settings for installed sublets if any
      #
      # @param [String]  name       Name of the Sublet
      # @param [Bool]    use_color  Use colors
      #
      # @raise [String] Config error
      # @since 0.2
      #
      # @example
      #   Sur::Client.new.config("sublet")
      #   => "Name     Type    Default value Description"
      #      "interval integer 60            Update interval in seconds"

      def config(name, use_color = true)
        build_local

        # Check if sublet is installed
        if (specs = search(name, @cache_local)) and !specs.empty?
          spec = specs.first

          show_config(spec, use_color)
          see_also(spec)
        end
      end # }}}

      ## Sur::Client::fetch {{{
      # Install a new Sublet
      #
      # @param [Array]   names     Names of Sublets
      # @param [String]  version   Version of the Sublet
      # @param [String]  use_tags  Use tags
      #
      # @raise [String] Fetch error
      # @since 0.0
      #
      # @example
      #   Sur::Client.new.fetch([ "sublet" ])
      #   => nil

      def fetch(names, version = nil, use_tags = false)
        build_remote

        # Install all sublets
        names.each do |name|
          # Check if remote sublet exists
          if (specs = search(name, @cache_remote, version, use_tags)) and
              specs.empty?
            puts ">>> WARNING: Cannot find sublet `#{name}' " \
                 "in remote repository"

            next
          end

          spec = specs.first

          # Download and copy sublet to current directory
          unless (temp = download(spec)).nil?
            FileUtils.cp(temp.path,
              File.join(
                Dir.getwd,
                "%s-%s.sublet" % [ spec.name.downcase, spec.version ]
              )
            )
          end
        end
      end # }}}

      ## Sur::Client::grabs {{{
      # Show grabs for installed sublets if any
      #
      # @param [String]  name       Name of the Sublet
      # @param [Bool]    use_color  Use colors
      #
      # @raise [String] Config error
      # @since 0.2
      #
      # @example
      #   Sur::Client.new.grabs("sublet")
      #   => "Name        Description"
      #      "SubletTest  Test grabs"

      def grabs(name, use_color = true)
        build_local

        # Check if sublet is installed
        if (specs = search(name, @cache_local)) and !specs.empty?
          spec = specs.first

          show_grabs(spec, use_color)
          see_also(spec)
        end
      end # }}}

      ## Sur::Client::info {{{
      # Show info for installed sublets if any
      #
      # @param [Array]   names      Names of the Sublets
      # @param [Bool]    use_color  Use colors
      #
      # @raise [String] Config error
      # @since 0.2
      #
      # @example
      #   Sur::Client.new.info("sublet")
      #   => "Name        Description"
      #      "SubletTest  Test grabs"

      def info(names, use_color = true)
        build_local

        # Show info for all sublets
        names.each do |name|
          # Check if sublet is installed
          if (specs = search(name, @cache_local)) and !specs.empty?
            spec = specs.first

            show_info(spec, use_color)
            see_also(spec)
          end
        end
      end # }}}

      ## Sur::Client::install {{{
      # Install a new Sublet
      #
      # @param [Array]   names     Names of Sublets
      # @param [String]  version   Version of the Sublet
      # @param [String]  use_tags  Use tags
      # @param [Bool]    reload    Reload sublets after installing
      #
      # @raise [String] Install error
      # @since 0.0
      #
      # @example
      #   Sur::Client.new.install([ "sublet" ])
      #   => nil

      def install(names, version = nil, use_tags = false, reload = false)
        build_local
        build_remote

        # Install all sublets
        names.each do |name|
          # Check if sublet is already installed
          if (specs = search(name, @cache_local, version, use_tags)) and
              !specs.empty?
            puts ">>> WARNING: Sublet `#{name}' is already installed"

            next
          end

          # Check if sublet is local
          if File.exist?(name)
            install_sublet(name)

            next
          end

          # Check if remote sublet exists
          if (specs = search(name, @cache_remote, version, use_tags)) and
              specs.empty?
            puts ">>> WARNING: Cannot find sublet `#{name}' " \
                 "in remote repository"

            next
          end

          # Check dependencies
          spec = specs.first
          next unless spec.satisfied?

          # Download and install sublet
          unless (temp = download(spec)).nil?
            install_sublet(temp.path)
          end
        end

        build_local(true)

        reload_sublets if reload
      end # }}}

      ## Sur::Client::list {{{
      # List the Sublet in a repository
      #
      # @param [String]  repo       Repo name
      # @param [Bool]    use_color  Use colors
      #
      # @since 0.0
      #
      # @example
      #   Sur::Client.new.list("remote")
      #   => nil

      def list(repo, use_color = true)
        # Select cache
        case repo
          when "local"
            build_local
            specs = @cache_local
          when "remote"
            build_local
            build_remote
            specs = @cache_remote
        end

        show_list(specs, use_color)
      end # }}}

      ## Sur::Client::notes {{{
      # Show notes about an installed sublet if any
      #
      # @param [String]  name  Name of the Sublet
      #
      # @raise [String] Notes error
      # @since 0.2
      #
      # @example
      #   Sur::Client.new.notes("sublet")
      #   => "Notes"

      def notes(name)
        build_local

        # Check if sublet is installed
        if (specs = search(name, @cache_local)) and !specs.empty?
          spec = specs.first

          show_notes(spec)
          see_also(spec)
        end
      end # }}}

      ## Sur::Client::query {{{
      # Query repo for a Sublet
      #
      # @param [String]  query      Query string
      # @param [String]  repo       Repo name
      # @param [Bool]    use_regex  Use regex
      # @param [Bool]    use_tags   Use tags
      # @param [Bool]    use_color  Use colors
      #
      # @raise [String] Query error
      # @since 0.0
      #
      # @example
      #   Sur::Client.new.query("sublet", "remote")
      #   => nil

      def query(query, repo, version = nil, use_regex = false, use_tags = false, use_color = true)
        case repo
          when "local"
            build_local
            unless (specs = search(query, @cache_local, version,
                use_regex, use_tags)) and !specs.empty?
              raise "Cannot find `#{query}' in local repository"
            end
          when "remote"
            build_local
            build_remote
            unless (specs = search(query, @cache_remote, version,
                use_regex, use_tags)) and !specs.empty?
              raise "Cannot find `#{query}' in remote repository"
            end
        end

        show_list(specs, use_color)
      end # }}}

      ## Sur::Client::reorder {{{
      # Reorder install Sublet
      #
      # @example
      #   Sur::Client.new.reorder
      #   => nil

      def reorder
        build_local

        i     = 0
        list  = []
        files = Dir[@path_sublets + "/*"]

        # Show menu
        @cache_local.each do |s|
          puts "(%d) %s (%s)" % [ i + 1, s.name, s.version ]

          # Check for match if sublet was reordered
          files.each do |f|
            s.files.each do |sf|
              a = File.basename(f)
              b = File.basename(sf)

              if a == b or File.fnmatch("[0-9_]*#{b}", a)
                list.push([ s.name.downcase, a])
              end
            end
          end

          i += 1
        end

        # Get new order
        puts "\n>>> Enter new numbers separated by blanks:\n"
        printf ">>> "

        line = STDIN.readline
        i    = 0

        if "\n" != line
          line.split(" ").each do |tok|
            idx = tok.to_i

            name, file = list.at(idx -1)

            i        += 10
            new_path  = '%s/%d_%s.rb' % [ @path_sublets, i, name ]

            # Check if file exists before moving
            unless File.exist?(new_path)
              FileUtils.mv(File.join(@path_sublets, file), new_path)
            end
          end

          build_local(true) #< Update local cache
        end
      end # }}}

      ## Sur::Client::submit {{{
      # Submit a Sublet to a repository
      #
      # @param [String]  file  Sublet package
      #
      # @raise [String] Submit error
      # @since 0.0
      #
      # @example
      #   Sur::Client.new.submit("sublet")
      #   => nil

      def submit(file)
        if !file.nil? and File.file?(file) and ".sublet" == File.extname(file)
          spec = Sur::Specification.extract_spec(file)

          if spec.valid?
            upload(file)
            build_remote(true)
          else
            spec.validate
          end
        else
          raise "Cannot find file `#{file}'"
        end
      end # }}}

      ## Sur::Client::uninstall {{{
      # Uninstall a Sublet
      #
      # @param [Array]   names     Names of Sublets
      # @param [String]  version   Version of the Sublet
      # @param [Bool]    use_tags  Use tags
      # @param [Bool]    reload    Reload sublets after uninstalling
      #
      # @raise [String] Uninstall error
      # @since 0.0
      #
      # @example
      #   Sur::Client.new.uninstall("sublet")
      #   => nil

      def uninstall(names, version = nil, use_tags = false, reload = false)
        build_local

        # Install all sublets
        names.each do |name|
          # Check if sublet is installed
          if (specs = search(name, @cache_local, version, use_tags)) and
              !specs.empty?
            spec = specs.first

            # Uninstall files
            spec.files.each do |f|
              # Check for match if sublet was reordered
              Dir[@path_sublets + "/*"].each do |file|
                a = File.basename(f)
                b = File.basename(file)

                if a == b or File.fnmatch("[0-9_]*#{a}", b)
                  puts ">>>>>> Uninstalling file `#{b}'"
                  FileUtils.remove_file(File.join(@path_sublets, b), true)
                end
              end
            end

            # Uninstall icons
            spec.icons.each do |f|
              puts ">>>>>> Uninstalling icon `#{f}'"
              FileUtils.remove_file(
                File.join(@path_icons, File.basename(f)), true
              )
            end

            # Uninstall specification
            puts ">>>>>> Uninstalling specification `#{spec.to_s}.spec'"
            FileUtils.remove_file(
              File.join(@path_specs, spec.to_s + ".spec"),
              true
            )

            puts ">>> Uninstalled sublet #{spec.to_s}"
          else
            puts ">>> WARNING: Cannot find sublet `#{name}' in local " \
                 "repository"
          end
        end

        build_local(true)

        reload_sublets if reload
      end # }}}

      ## Sur::Client::unpack {{{
      # Unpack sublet to current path
      #
      # @param [Array]   names     Names of Sublets
      # @param [String]  version   Version of the Sublet
      # @param [Bool]    use_tags  Use tags
      #
      # @raise [String] Unpack error
      # @since 0.0
      #
      # @example
      #   Sur::Client.new.unpack("sublet")
      #   => nil

      def unpack(names, version = nil, use_tags = false)
        build_remote

        # Install all sublets
        names.each do |name|
          # Check if sublet is installed
          if (specs = search(name, @cache_remote, version, use_tags)) and
              !specs.empty?
            spec = specs.first

            # Download and unpack sublet
            unless (temp = download(spec)).nil?
              base  = File.join(Dir.pwd, spec.to_str)
              icons = File.join(base, "icons")

              FileUtils.mkdir_p([ icons ])

              install_sublet(temp.path, base, icons, base)
            end
          end
        end

        build_local(true)
      end # }}}

      ## Sur::Client::update {{{
      # Update a repository
      #
      # @param [String]  repo  Repo name
      #
      # @since 0.0
      #
      # @example
      #   Sur::Client.new.update("remote")
      #   => nil

      def update(repo)
        case repo
          when "local"
            build_local(true)
          when "remote"
            build_remote(true)
        end
      end # }}}

      ## Sur::Client::upgrade {{{
      # Upgrade all Sublets
      #
      # @param [Bool]  use_color  Use colors
      # @param [Bool]  reload     Reload sublets after uninstalling
      # @param [Bool]  assume     Whether to assume yes
      #
      # @raise Upgrade error
      # @since 0.2
      #
      # @example
      #   Sur::Client.new.upgrade
      #   => nil

      def upgrade(use_color = true, reload = false, assume = false)
        build_local
        build_remote

        list = []

        # Iterate over server-side sorted list
        @cache_local.each do |spec_a|
          @cache_remote.each do |spec_b|
            if spec_a.name == spec_b.name and
                spec_a.version.to_f < spec_b.version.to_f
              list << spec_b.name

              if use_color
                puts ">>> %s: %s -> %s" % [
                  spec_a.name,
                  colorize(5, spec_a.version),
                  colorize(2, spec_b.version)
                ]
              else
                 puts ">>> %s: %s -> %s" % [
                  spec_a.name, spec_a.version, spec_b.version
                ]
              end

              break
            end
          end
        end

        return if list.empty?

        # Really?
        unless assume
          print ">>> Upgrade sublets (y/n)? "

          return unless "y" == STDIN.readline.strip.downcase
        end

        # Finally upgrade
        uninstall(list)
        install(list)

        reload_sublets if reload
      end # }}}

      ## Sur::Client::yank {{{
      # Delete a sublet from remote server
      #
      # @param [String]  name     Name of the Sublet
      # @param [String]  version  Version of the Sublet
      #
      # @raise Upgrade error
      # @since 0.2
      #
      # @example
      #   Sur::Client.new.yank("subtle", "0.0")
      #   => nil

      def yank(use_color = true, reload = false, assume = false)
        raise NotImplementedError
      end # }}}

      private

      def see_also(spec) # {{{
        also = [ "info", "config" ]

        also << "notes" unless spec.notes.nil?
        also << "grabs" unless spec.grabs.nil?

        puts "See also: #{also.join(", ")}"
      end # }}}

      def upload(file) # {{{
        uri    = URI.parse(HOST + "/submit")
        http   = Net::HTTP.new(uri.host, uri.port)
        base   = File.basename(file)
        body   = ""

        # Assemble data
        body << "--#{BOUNDARY}\r\n"
        body << "Content-Disposition: form-data; name=\"user\"\r\n\r\n"
        body << ENV["USER"]
        body << "\r\n--#{BOUNDARY}\r\n"
        body << "Content-Disposition: form-data; name=\"file\"; filename=\"#{base}\"\r\n"
        body << "Content-Type: application/x-tar\r\n\r\n"
        body << File.read(file)
        body << "\r\n--#{BOUNDARY}--\r\n"

        # Send reqiest
        req = Net::HTTP::Post.new(uri.request_uri)
        req.body            = body
        req["Content-Type"] = "multipart/form-data; boundary=#{BOUNDARY}"

        # Check result
        case http.request(req).code.to_i
          when 200
             puts ">>> Submitted sublet `#{base}'"

            build_remote
          when 405 then raise "Cannot submit sublet: Invalid request"
          when 415 then raise "Cannot submit sublet: Invalid Spec"
          when 500 then raise "Cannot submit sublet: Server error"
          else raise "Cannot submit sublet"
        end
      end # }}}

      def download(spec) # {{{
        temp = nil
        uri  = URI.parse(HOST)

        # Proxy?
        begin
          proxy = URI.parse(ENV["HTTP_PROXY"])
          http  = Net::HTTP::Proxy(proxy.host, proxy.port,
            proxy.user, proxy.password).start(uri.host, uri.port)
        rescue
          http = Net::HTTP.new(uri.host, uri.port)
        end

        # Fetch file
        http.request_get("/get/" + spec.digest) do |response|
          # Check result
          case response.code.to_i
            when 200
              total = 0

              # Create tempfile
              temp = Tempfile.new(spec.to_s)

              # Write file content
              response.read_body do |str|
                total += str.length
                temp  << str

                progress(">>> Fetching sublet `#{spec.to_s}'",
                  response.content_length, total)
              end

              temp.close
            when 404 then raise "Cannot download sublet: Sublet not found"
            else raise "Cannot download sublet: Server error"
          end
        end

        puts

        return temp
      end # }}}

      def progress(mesg, total, now) # {{{
        if 0.0 < total
          percent = (now.to_f * 100 / total.to_f).floor

          $stdout.sync = true
          print "\r#{mesg} (#{percent}%)"
        end
      end # }}}

      def search(query, repo, version = nil, # {{{
          use_regex = false, use_tags = false)
        results = []

        # Search in repo
        repo.each do |s|
          if !query.nil? and (s.name.downcase == query.downcase or
              (use_regex and s.name.downcase.match(query)) or
              (use_tags  and s.tags.include?(query.capitalize)))
            # Check version?
            if version.nil? or s.version == version
              results.push(s)
            end
          end
        end

        results
      end # }}}

      def colorize(color, text, bold = false, mode = :fg) # {{{
        c = true == bold ? 1 : 30 + color  #< Bold mode
        m = :fg == mode  ? 1 :  3 #< Color mode

        "\033[#{m};#{c}m#{text}\033[m"
      end # }}}

      def build_local(force = false) # {{{
        @cache_local = []

        # Load local cache
        if !force and File.exist?(@path_local)
          yaml = YAML::load(File.open(@path_local))

          @cache_local = YAML::load(yaml)

          return
        end

        # Check installed sublets
        Dir[@path_specs + "/*"].each do |file|
          begin
            spec = Sur::Specification.load_spec(file)

            # Validate
            if spec.valid?
              spec.path = file
              @cache_local.push(spec)
            else
              spec.validate
            end
          rescue
            puts ">>> WARNING: Cannot parse specification `#{file}'"
          end
        end

        puts ">>> Updated local cache with #{@cache_local.size} entries"

        @cache_local.sort { |a, b| [ a.name, a.version ] <=> [ b.name, b.version ] }

        # Dump yaml file
        yaml = @cache_local.to_yaml
        File.open(@path_local, "w+") do |out|
          YAML::dump(yaml, out)
        end
      end # }}}

      def build_remote(force = false) # {{{
        @cache_remote = []
        uri           = URI.parse(HOST)
        http          = Net::HTTP.new(uri.host, uri.port)

        # Check age of cache
        if !force and File.exist?(@path_remote) and
            86400 > (Time.now - File.new(@path_remote).ctime)
          yaml = YAML::load(File.open(@path_remote))

          @cache_remote = YAML::load(yaml)

          return
        end

        # Fetch file
        http.request_get("/list") do |response|
          # Check result
          case response.code.to_i
            when 200
              total = 0
              data  = ""

              # Downloading list
              response.read_body do |str|
                total += str.length
                data  << str

                progress(">>> Fetching list", response.content_length, total)
              end

              puts

              # Reading list
              @cache_remote = YAML::load(YAML::load(data))

              @cache_remote.sort do |a, b|
                [ a.name, a.version ] <=> [ b.name, b.version ]
              end

              # Dump yaml file
              yaml = @cache_remote.to_yaml
              File.open(@path_remote, "w+") do |out|
                YAML::dump(yaml, out)
              end

              puts ">>> Updated remote cache with #{@cache_remote.size} entries"
            else raise "Cannot download sublet list: Server error"
          end
        end
      end # }}}

      def compact_list(specs) # {{{
        list = []

        # Skip if specs list is empty
        if specs.any?
          prev = nil

          specs.sort { |a, b| [ a.name, a.version ] <=> [ b.name, b.version ] }.reverse!

          # Compress versions
          specs.each do |s|
            if !prev.nil? and prev.name == s.name
              if prev.version.is_a?(Array)
                prev.version << s.version
              else
                prev.version = [ prev.version, s.version ]
              end
            else
              list << prev unless prev.nil?
              prev = s
            end
          end

          list << prev unless prev.nil? and prev.name == s.name
        end

        list
      end # }}}

      def show_list(specs, use_color) # {{{
        specs = compact_list(specs)
        i     = 1

        specs.each do |s|
          # Find if installed
          installed = ""
          @cache_local.each do |cs|
            if cs.name == s.name
              installed = "[%s installed]" % [ cs.version ]
              break
            end
          end

          # Convert version array to string
          version = s.version.is_a?(Array) ? s.version.join(", ") : s.version

          # Do we like colors?
          if use_color
            puts "%s %s (%s) %s" % [
              colorize(2, i.to_s, false, :bg),
              colorize(1, s.name.downcase, true),
              colorize(2, version),
              colorize(2, installed, false, :bg)
            ]
            puts "   %s" % [ s.description ]

            unless s.tags.empty?
              puts "   %s" % [ s.tags.map { |t| colorize(5, "##{t}") }.join(" ") ]
            end
          else
            puts "(%d) %s (%s) %s" % [ i, s.name.downcase, version, installed ]
            puts "   %s" % [ s.description ]

            unless s.tags.empty?
              puts "   %s" % [ s.tags.map { |t| "##{t}" }.join(" ") ]
            end
          end

          i += 1
        end
      end # }}}

      def show_notes(spec) # {{{
        unless spec.notes.nil? or spec.notes.empty?
          puts
          puts spec.notes
          puts
        end
      end # }}}

      def show_config(spec, use_color) # {{{
        unless spec.nil?
          puts

          # Add default config settings
          config = [
            {
              :name        => "interval",
              :type        => "integer",
              :description => "Update interval in seconds",
              :def_value   => "60"
            },
            {
              :name        => "style",
              :type        => "string",
              :description => "Default sublet style (sub-style of sublets)",
              :def_value   => "Sublet name"
            }

          ]

          # Merge configs
          unless spec.config.nil?
            skip = []

            spec.config.each do |c|
              case c[:name]
                when "interval" then skip << "interval"
                when "style"    then skip << "style"
              end
            end

            config.delete_if { |c| skip.include?(c[:name]) }

            config |= spec.config
          end

          # Header
          if use_color
            puts "%-24s  %-19s  %-39s  %s" % [
              colorize(1, "Name", true),
              colorize(1, "Type", true),
              colorize(1, "Default value", true),
              colorize(1, "Description", true)
            ]
          else
            puts "%-15s  %-10s  %-30s  %s" % [
              "Name", "Type", "Default value", "Description"
            ]
          end

          # Dump all settings
          config.each do |c|
            if use_color
              puts "%-25s  %-20s  %-40s  %s" % [
                colorize(2, c[:name][0..24]).ljust(25),
                colorize(5, c[:type][0..19]).ljust(20),
                colorize(3, c[:def_value]) || "",
                c[:description]
              ]
            else
              puts "%-14s  %9s  %-30s  %s" % [
                c[:name][0..15].ljust(15),
                c[:type][0..10].ljust(10),
                c[:def_value] || "",
                c[:description]
              ]
            end
          end

          puts
        end
      end # }}}

      def show_grabs(spec, use_color) # {{{
        unless spec.nil? or spec.grabs.nil? or spec.grabs.empty?
          puts

          # Header
          if use_color
            puts "%-24s  %s" % [
              colorize(1, "Name", true),
              colorize(1, "Description", true)
            ]
          else
            puts "%-15s  %s" % [
              "Name", "Description"
            ]
          end

          # Dump all settings
          spec.grabs.each do |k, v|
            if use_color
              puts "%-25s  %s" % [
                colorize(2, k[0..24]).ljust(25), v
              ]
            else
              puts "%-14s  %s" % [
                k[0..15].ljust(15), v
              ]
            end
          end

          puts
        end
      end # }}}

      def show_info(spec, use_color) # {{{
        unless spec.nil?
          authors = spec.authors.join(", ")
          tags    = spec.tags.map { |t| "##{t}" }.join(" ")
          files   = spec.files.join(", ")
          icons   = spec.icons.map { |i| File.basename(i) }.join(", ")
          deps    = spec.dependencies.map { |k, v| "#{k} (#{v})" }.join(", ")

          if use_color
            puts <<-EOF

#{colorize(1, "Name:", true)}     #{spec.name}
#{colorize(1, "Version:", true)}  #{colorize(2, spec.version)}
#{colorize(1, "Authors:", true)}  #{authors}
#{colorize(1, "Contact:", true)}  #{colorize(6, spec.contact)}
#{colorize(1, "Tags:", true)}     #{colorize(5, tags)}
#{colorize(1, "Files:", true)}    #{files}
#{colorize(1, "Icons:", true)}    #{icons}
#{colorize(1, "Deps:", true)}     #{spec.dependencies.map { |k, v| "#{k} (#{colorize(2, v)})" }.join(", ")}

            EOF
          else
            puts <<-EOF

Name:     #{spec.name}
Version:  #{spec.version}
Authors:  #{authors}
Contact:  #{spec.contact}
Tags:     #{tags}
Files:    #{files}
Icons:    #{icons}
Deps:     #{spec.dependencies.map { |k, v| "#{k} (#{v})" }.join(", ")}

            EOF
          end
        end
      end # }}}

      def install_sublet(sublet, specs = @path_specs, # {{{
          icons = @path_icons, sublets = @path_sublets)
        spec = Sur::Specification.extract_spec(sublet)

        # Open sublet and install files
        File.open(sublet, "rb") do |input|
          Archive::Tar::Minitar::Input.open(input) do |tar|
            tar.each do |entry|
              case File.extname(entry.full_name)
                when ".spec" then
                  puts ">>>>>> Installing specification `#{spec.to_s}.spec'"
                  path = File.join(specs, spec.to_s + ".spec")
                when ".xbm", ".xpm" then
                  puts ">>>>>> Installing icon `#{entry.full_name}'"
                  path = File.join(icons, entry.full_name)
                else
                  puts ">>>>>> Installing file `#{entry.full_name}'"
                  path = File.join(sublets, entry.full_name)
              end

              # Install file
              File.open(path, "w+") do |output|
                output.write(entry.read)
              end
            end
          end

          puts ">>> Installed sublet #{spec.to_str}"

          show_notes(spec)
        end
      end # }}}

      def reload_sublets # {{{
        begin
          require "subtle/subtlext"

          Subtlext::Subtle.reload
        rescue
          raise "Cannot reload sublets"
        end
      end # }}}
    end # }}}
  end # }}}
end # }}}

# vim:ts=2:bs=2:sw=2:et:fdm=marker

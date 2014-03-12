# -*- encoding: utf-8 -*-
#
# @package sur
#
# @file Specification functions
# @author Christoph Kappel <unexist@subforge.org>
# @version $Id: data/sur/specification.rb,v 3182 2012/02/04 16:39:33 unexist $
#
# This program can be distributed under the terms of the GNU GPLv2.
# See the file COPYING for details.
#

require "archive/tar/minitar"
require "rubygems"

module Subtle # {{{
  module Sur # {{{
    # Specification class for specs
    class Specification # {{{
      # Sublet name
      attr_accessor :name

      # Sublet version
      attr_accessor :version

      # Tag list
      attr_accessor :tags

      # File list
      attr_accessor :files

      # Icon list
      attr_accessor :icons

      # Short description
      attr_accessor :description

      # Notes
      attr_accessor :notes

      # Author list
      attr_accessor :authors

      # Contact address
      attr_accessor :contact

      # Date of creation
      attr_accessor :date

      # Config values
      attr_accessor :config

      # Provided grabs
      attr_accessor :grabs

      # Hash of dependencies
      attr_accessor :dependencies

      # Version of subtle
      attr_accessor :required_version

      # Path of spec
      attr_accessor :path

      # Digest
      attr_accessor :digest

      ## Subtle::Sur::Specification::initialize {{{
      # Create a new Specification object
      #
      # @yield [self] Init block
      # @yieldparam [Object]  self  Self instance
      # @return [Object] New Specification
      #
      # @since 0.1
      #
      # @example
      #   Subtle::Sur::Specification.new
      #   => #<Sur::Specification:xxx>

      def initialize
        @name             = "Unnamed"
        @authors          = []
        @dependencies     = {}
        @subtlext_version = nil
        @sur_version      = nil
        @files            = []
        @icons            = []
        @path             = Dir.pwd

        # Pass to block
        yield(self) if block_given?
      end # }}}

      ## Subtle::Sur::Specification::load_spec {{{
      # Load Specification from file
      #
      # @param [String]  file  Specification file name
      # @return [Object] New Specification
      #
      # @raise [String] Loading error
      # @since 0.1
      #
      # @example
      #   Subtle::Sur::Specification.load_file("sublet.spec")
      #   => #<Sur::Specification:xxx>

      def self.load_spec(file)
        spec = nil

        # Catch IO exceptions
        begin
          # Create spec
          spec      = eval(File.open(file).read)
          spec.path = File.dirname(file)
        rescue Exception => e
          spec = nil
        end

        # Check object
        if spec.nil? or !spec.is_a?(Specification)
          raise "Cannot read specification file `#{file}'"
        elsif !spec.valid?
          raise spec.validate
        end

        spec
      end # }}}

      ## Subtle::Sur::Specification::extract_spec {{{
      # Extract and load Specification from file
      #
      # @param [String]  file  Tar file name
      # @return [Object] New Specification
      #
      # @raise [String] Loading error
      # @see Subtle::Sur::Specification.load_spec
      # @since 0.1
      #
      # @example
      #   Subtle::Sur::Specification.extract_spec("foo.sublet")
      #   => #<Sur::Specification:xxx>

      def self.extract_spec(file)
        spec = nil

        begin
          File.open(file, "rb") do |input|
            Archive::Tar::Minitar::Input.open(input) do |tar|
              tar.each do |entry|
                if ".spec" == File.extname(entry.full_name)
                  spec = eval(entry.read)
                end
              end
            end
          end
        rescue
          spec = nil
        end

        # Check object
        if spec.nil? or !spec.is_a?(Specification)
          raise "Cannot extract specification from file `#{file}'"
        elsif !spec.valid?
          raise spec.validate
        end

        spec
      end # }}}

      ## Subtle::Sur::Specification::template {{{
      # Create a new Specification object
      #
      # @param [String]  file  Template name
      #
      # @since 0.1
      #
      # @example
      #   Subtle::Sur::Specification.template("foobar")
      #   => nil

      def self.template(file)
        require "subtle/sur/version"

        # Build filenname
        ext     = File.extname(file)
        ext     = ".rb" if(ext.empty?)
        name    = File.basename(file, ext)

        folder  = File.join(Dir.pwd, name)
        spec    = File.join(folder, name + ".spec")
        sublet  = File.join(folder, name + ".rb")

        unless File.exist?(name)
          FileUtils.mkdir_p([ folder ])

          # Create spec
          File.open(spec, "w+") do |output|
            output.puts <<EOF
# -*- encoding: utf-8 -*-
# #{name.capitalize} specification file
# Created with sur-#{Subtle::Sur::VERSION}
Sur::Specification.new do |s|
  # Sublet information
  s.name        = "#{name.capitalize}"
  s.version     = "0.0"
  s.tags        = [ ]
  s.files       = [ "#{name.downcase}.rb" ]
  s.icons       = [ ]

  # Sublet description
  s.description = "SHORT DESCRIPTION"
  s.notes       = <<NOTES
LONG DESCRIPTION
NOTES

  # Sublet authors
  s.authors     = [ "#{ENV["USER"]}" ]
  s.contact     = "YOUREMAIL"
  s.date        = "#{Time.now.strftime("%a %b %d %H:%M %Z %Y")}"

  # Sublet config
  #s.config = [
  #  {
  #    :name        => "Value name",
  #    :type        => "Value type",
  #    :description => "Description",
  #    :def_value   => "Default value"
  #  }
  #]

  # Sublet grabs
  #s.grabs = {
  #  :#{name.capitalize}Grab => "Sublet grab"
  #}

  # Sublet requirements
  # s.required_version = "0.9.2127"
  # s.add_dependency("subtle", "~> 0.1.2")
end
EOF
          end

          # Create sublet
          File.open(sublet, "w+") do |output|
            output.puts <<EOF
# #{name.capitalize} sublet file
# Created with sur-#{Subtle::Sur::VERSION}
configure :#{name} do |s|
  s.interval = 60
end

on :run do |s|
  s.data =
end
EOF
          end

          puts ">>> Created template for `#{name}'"
        else
          raise "File `%s' already exists" % [ name ]
        end
      end # }}}

      ## Subtle::Sur::Specification::valid? {{{
      # Checks if a specification is valid
      #
      # @return [true, false] Validity of the package
      #
      # @since 0.1
      #
      # @example
      #   Subtle::Sur::Specification.new.valid?
      #   => true

      def valid?
        (!@name.nil? and !@authors.empty? and !@contact.nil? and \
          !@description.nil? and !@version.nil? and !@files.empty?)
      end # }}}

      ## Subtle::Sur::Specification::validate {{{
      # Check if a specification is valid
      #
      # @raise [String] Validation error
      # @since 0.1
      #
      # @example
      #   Subtle::Sur::Specification.new.validate
      #   => nil

      def validate
        fields = []

        # Check required fields of spec
        fields.push("name")        if(@name.nil?)
        fields.push("authors")     if(@authors.empty?)
        fields.push("contact")     if(@contact.nil?)
        fields.push("description") if(@description.nil?)
        fields.push("version")     if(@version.nil?)
        fields.push("files")       if(@files.empty?)

        unless fields.empty?
          raise SpecificationValidationError,
            "Couldn't find `#{fields.join(", ")}' in specification"
        end
      end # }}}

      ## Subtle::Sur::Specification::add_dependency {{{
      # Add a gem dependency to the package
      #
      # @param [String]  name     Dependency name
      # @param [String]  version  Required version
      #
      # @since 0.1
      #
      # @example
      #   Subtle::Sur::Specification.new.add_dependency("subtle", "0.8")
      #   => nil

      def add_dependency(name, version)
        @dependencies[name] = version
      end # }}}

      ## Subtle::Sur::Specification::satisfied? {{{
      # Check if all dependencies are satisfied
      #
      # @return [true, false] Validity of the package
      #
      # @since 0.1
      #
      # @example
      #   Subtle::Sur::Specification.new.satisfied?
      #   => true

      def satisfied?
        satisfied_version = true
        satisfied_deps    = true
        missing   = []

        # Check subtlext version
        unless @required_version.nil?
          begin
            require "subtle/subtlext"

            # Check subtlext version
            major_have, minor_have, teeny_have = Subtlext::VERSION.split(".").map(&:to_i)
            major_need, minor_need, teeny_need = @required_version.split(".").map(&:to_i)

            if(major_need > major_have or minor_need > minor_have or
               teeny_need.nil? or teeny_have.nil? or teeny_need > teeny_have)
              satisfied_version = false
            end
          rescue => err
            puts ">>> ERROR: Couldn't verify version of subtle"

            satisfied_version = false
          end
        end

        # Check gem dependencies
        @dependencies.each do |k, v|
          begin
            gem(k, v)
          rescue Gem::LoadError
            # Try to load it
            begin
              require k

              puts ">>> WARNING: Couldn't verify version of `#{k}' with rubygems"
            rescue LoadError
              missing.push("%s (%s)" % [ k, v])

              satisfied_deps = false
            end
          end
        end

        # Dump errors
        unless satisfied_version or satisfied_deps
          puts ">>> ERROR: Couldn't install `#{@name}' due to unsatisfied requirements."

          unless(satisfied_version)
            puts "           Subtle >=#{@required_version} (found: #{Subtlext::VERSION}) is required."
          end

          unless(missing.empty?)
            puts "           Following gems are missing: #{missing.join(", ")}"
          end
        end

        satisfied_version and satisfied_deps
      end # }}}

      ## Subtle::Sur::Specification::to_str {{{
      # Convert Specification to string
      #
      # @return [String] Specification as string
      #
      # @since 0.1
      #
      # @example
      #   Subtle::Sur::Specification.new.to_str
      #   => "sublet-0.1"

      def to_str
        "#{@name}-#{@version}".downcase
      end # }}}

      ## Subtle::Sur::Specification::method_missing {{{
      # Dispatcher for Specification instance
      #
      # @param [Symbol]  meth  Method name
      # @param [Array]   args  Argument array
      #
      # @since 0.2

      def method_missing(meth, *args)
        ret = nil

        # Check if symbol is a method or a var
        if self.respond_to?(meth)
          ret = self.send(meth, args)
        else
          sym = ("@" + meth.to_s).to_sym #< Construct symbol

          # Check getter or setter
          if(meth.to_s.index("="))
            sym = sym.to_s.chop.to_sym

            self.instance_variable_set(sym, args.first)
          else
            ret = self.instance_variable_get(sym)
            ret = self if(ret.nil?)
          end
        end

        ret
      end # }}}

      # Aliases
      alias :to_s :to_str

      # FIXME: Deprecated
      alias :subtlext_version :required_version
      alias :subtlext_version= :required_version=
    end # }}}
  end # }}}
end # }}}

# vim:ts=2:bs=2:sw=2:et:fdm=marker

# -*- encoding: utf-8 -*-
#
# @package subtle
#
# @file Rake build file
# @copyright (c) 2005-2012 Christoph Kappel <unexist@subforge.org>
# @version $Id$
#
# This program can be distributed under the terms of the GNU GPL.
# See the file COPYING.
#

require "mkmf"
require "yaml"
require "rake/clean"
require "digest/md5"

# FIXME: RDoc version crap
begin
  require "rdoc/task"
rescue LoadError
  require "rake/rdoctask"

  module RDoc
    class Task < Rake::RDocTask
    end
  end
end

# FIXME: mkmf redefines #rm_f for splats and rake (>=0.9.1)
# includes FileUtils and just dispatches to it
module Rake
  module FileUtilsExt
    def rm_f(*files)
      # Copied from mkmf.rb:197
      opt = (Hash === files.last ? [files.pop] : [])
      FileUtils.rm_f(Dir[*files.flatten], *opt)
    end
  end
end

# Settings

# Options / defines {{{
@options = {
  "cc"         => ENV["CC"] || "gcc",
  "destdir"    => ENV["DESTDIR"] || "",
  "prefix"     => "/usr",
  "manprefix"  => "$(prefix)/share/man",
  "bindir"     => "$(destdir)/$(prefix)/bin",
  "sysconfdir" => "$(destdir)/etc",
  "configdir"  => "$(sysconfdir)/xdg/$(PKG_NAME)",
  "datadir"    => "$(destdir)/$(prefix)/share/$(PKG_NAME)",
  "extdir"     => "$(destdir)/$(sitelibdir)/$(PKG_NAME)",
  "mandir"     => "$(destdir)/$(manprefix)/man1",
  "debug"      => "no",
  "xpm"        => "yes",
  "xft"        => "yes",
  "xinerama"   => "yes",
  "xrandr"     => "yes",
  "xtest"      => "yes",
  "builddir"   => "build",
  "hdrdir"     => "",
  "archdir"    => "",
  "revision"   => "3224", #< Latest stable
  "cflags"     => "-Wall -Werror -Wpointer-arith -Wstrict-prototypes -Wunused -Wshadow -std=gnu99",
  "cpppath"    => "-I. -I$(builddir) -Isrc -Isrc/shared -Isrc/subtle -idirafter$(hdrdir) -idirafter$(archdir)",
  "ldflags"    => "-L$(libdir) $(rpath) $(LIBS) -l$(RUBY_SO_NAME)",
  "extflags"   => "$(LDFLAGS) $(rpath) $(LIBS) -l$(RUBY_SO_NAME)",
  "rpath"      => "-L$(libdir) -Wl,-rpath=$(libdir)",
  "checksums"  => []
}

@defines = {
  "PKG_NAME"      => "subtle",
  "PKG_VERSION"   => "0.11.$(revision)",
  "PKG_BUGREPORT" => "http://subforge.org/projects/subtle/issues",
  "PKG_CONFIG"    => "subtle.rb",
  "RUBY_VERSION"  => "$(MAJOR).$(MINOR).$(TEENY)"
}  # }}}

# Lists {{{
PG_SUBTLE   = "subtle"
PG_SUBTLEXT = "subtlext"
PG_SUBTLER  = "subtler"
PG_SUR      = "sur"
PG_SERVER   = "surserver"

SRC_SHARED   = FileList["src/shared/*.c"]
SRC_SUBTLE   = (SRC_SHARED | FileList["src/subtle/*.c"])
SRC_SUBTLEXT = (SRC_SHARED | FileList["src/subtlext/*.c"])

# Collect object files
OBJ_SUBTLE = SRC_SUBTLE.collect do |f|
  File.join(@options["builddir"], "subtle", File.basename(f).ext("o"))
end

OBJ_SUBTLEXT = SRC_SUBTLEXT.collect do |f|
  File.join(@options["builddir"], "subtlext", File.basename(f).ext("o"))
end

FUNCS   = [ "select" ]
HEADER  = [
  "stdio.h", "stdlib.h", "stdarg.h", "string.h", "unistd.h", "signal.h", "errno.h",
  "assert.h", "sys/time.h", "sys/types.h"
]
OPTIONAL = [ "sys/inotify.h", "wordexp.h" ]
# }}}

# Miscellaneous {{{
Logging.logfile("config.log") #< mkmf log
CLEAN.include(PG_SUBTLE, "#{PG_SUBTLEXT}.so", OBJ_SUBTLE, OBJ_SUBTLEXT)
CLOBBER.include(@options["builddir"], "config.h", "config.log", "config.yml")
# }}}

# Funcs

 ## silent_sh {{{
 # Wraper to suppress the output of shell commands
 # @param  [String]  cmd    Command name
 # @param  [String]  msg    Command replacement message
 # @param  [Block]   block  Command block
 ##

def silent_sh(cmd, msg, &block)
  # FIXME: Hide raw messages in 0.8.7 and 0.9.1
  unless true === RakeFileUtils.verbose or
      (defined? Rake::FileUtilsExt and
      Rake::FileUtilsExt.respond_to? :verbose_flag and
      true === Rake::FileUtilsExt.verbose_flag)
    rake_output_message(msg)
  else
    rake_output_message(Array == cmd.class ? cmd.join(" ") : cmd) #< Check type
  end

  unless RakeFileUtils.nowrite
    res = system(cmd)
    block.call(res, $?)
  end
end # }}}

 ## make_header {{{
 # Create config header
 # @param [String]  header  Name of the header
 ##

def make_header(header = "config.h")
  message "creating %s\n", header

  sym = header.tr("a-z./\055", "A-Z___")
  hdr = ["#ifndef #{sym}\n#define #{sym}\n"]

  # Create lines from defitions
  for line in $defs
    case line
    when /^-D([^=]+)(?:=(.*))?/
      hdr << "#define #$1 #{$2 ? $2 : 1}\n"
    when /^-U(.*)/
      hdr << "#undef #$1\n"
    end
  end
  hdr << "#endif\n"
  hdr = hdr.join

  unless(IO.read(header) == hdr rescue false)
    open(header, "w") do |hfile|
      hfile.write(hdr)
    end
  end
end # }}}

 ## make_config {{{
 # Create config file
 # @param [String]  file  Name of config file
 ##

def make_config(file = "config.yml")
  message("Creating %s\n" % [ file ])

  yaml = [@options, @defines].to_yaml

  # Dump yaml
  File.open(file, "w+") do |out|
    YAML::dump(yaml, out)
  end
end # }}}

 ## compile {{{
 # Wrapper to suppress compiler output
 # @param  [String]  src      Input filename
 # @param  [String]  out      Output filename
 # @param  [String]  options  Addiotional compiler options
 ##

def compile(src, out = nil, options = "")
  out = File.join(@options["builddir"], File.basename(src).ext("o")) if out.nil?
  opt = ["shared.c", "subtlext.c"].include?(File.basename(src)) ? " -fPIC " : ""
  opt << options

  # Suppress default output
  silent_sh("#{@options["cc"]} -o #{out} -c #{@options["cflags"]} #{opt} #{@options["cpppath"]} #{src}",
    "CC #{out}") do |ok, status|
      ok or fail("Compiler failed with status #{status.exitstatus}")
  end
end # }}}

 ## checksums # {{{
 # Create and check checksums of header files
 ##

def checksums
  ret = true

  # Create checksums
  files = Dir["src/*/*.h"]
  sums  = files.map { |h| Digest::MD5.file(h).to_s }

  # Call clean task when sums don't match
  if((@options["checksums"] - sums).any? rescue true)
    Rake::Task["clean"].invoke
    ret = false
  end

  @options["checksums"] = sums

  ret
end # }}}

# Tasks

 ## default {{{
 # Default task for rake
 ##

desc("Configure and build subtle")
task(:default => [:config, :build])
# }}}

 ## config {{{
 # Configure build environment
 ##

desc("Configure subtle")
task(:config) do
  # Check if build dirs exist
  [
    File.join(@options["builddir"], "subtle"),
    File.join(@options["builddir"], "subtlext")
  ].each do |dir|
    FileUtils.mkdir_p(dir) unless File.exist?(dir)
  end

  # Check if options.yaml exists or config is run explicitly
  if( !ARGV.nil? and !ARGV.include?("config")) and File.exist?("config.yml")
    yaml = YAML::load(File.open("config.yml"))
    @options, @defines = YAML::load(yaml)

    make_config unless checksums
  else
    # Check version
    if 1 != RbConfig::CONFIG["MAJOR"].to_i or 9 != RbConfig::CONFIG["MINOR"].to_i
      fail("Ruby 1.9.0 or higher required")
    end

    checksums

    # Update rpath
    @options["rpath"] = RbConfig.expand(@options["rpath"])

    # Get options
    @options.each_key do |k|
      unless ENV[k].nil?
        @options[k] = ENV[k]
      end
    end

    # Debug
    if "yes" == @options["debug"]
      @options["cflags"] << " -g -DDEBUG"
    else
      @options["cflags"] << " -DNDEBUG"
    end

    # Get revision
    if File.exists?(".hg") and (hg = find_executable0("hg"))
      match = `#{hg} tip`.match(/^[^:]+:\s*(\d+).*/)

      if !match.nil? and 2 == match.size
        @options["revision"] = match[1]
      end
    end

    # Get ruby header dir
    if RbConfig::CONFIG["rubyhdrdir"].nil?
      @options["hdrdir"] = RbConfig.expand(
        RbConfig::CONFIG["archdir"]
      )
    else
      @options["hdrdir"] = RbConfig.expand(
        RbConfig::CONFIG["rubyhdrdir"]
      )
    end

    @options["archdir"] = File.join(
      @options["hdrdir"],
      RbConfig.expand(RbConfig::CONFIG["arch"])
    )

    # Expand options and defines
    @options["sitelibdir"] = RbConfig.expand(
      RbConfig::CONFIG["sitelibdir"]
    )
    [@options, @defines].each do |hash|
      hash.each do |k, v|
        if v.is_a?(String)
          hash[k] = RbConfig.expand(
            v, CONFIG.merge(@options.merge(@defines))
          )
        end
      end
    end

    # Check arch
    if RbConfig::CONFIG["arch"].match(/openbsd/)
      $defs.push("-DIS_OPENBSD")
    end

    # Check header
    HEADER.each do |h|
      fail("Header #{h} was not found") unless have_header(h)
    end

    # Check optional headers
    OPTIONAL.each do |h|
      have_header(h)
    end

    # Check execinfo
    checking_for("execinfo.h") do
      ret = false
      lib = " -lexecinfo"

      # Check if execinfo is a separate lib (freebsd)
      if find_header("execinfo.h")
        if try_func("backtrace", "")
          $defs.push("-DHAVE_EXECINFO_H")

          ret = true
        elsif try_func("backtrace", lib)
          @options["ldflags"] << lib
          $defs.push("-DHAVE_EXECINFO_H")

          ret = true
        end
      end

      ret
    end

    # Check pkg-config for X11
    checking_for("X11/Xlib.h") do
      cflags, ldflags, libs = pkg_config("x11")
      fail("X11 was not found") if libs.nil?

      # Update flags
      @options["cflags"]   << " %s" % [ cflags ]
      @options["ldflags"]  << " %s %s" % [ ldflags, libs ]
      @options["extflags"] << " %s %s" % [ ldflags, libs ]

      true
    end

    # Check pkg-config for Xpm
    if "yes" == @options["xpm"]
      checking_for("X11/Xpm.h") do
        ret = false

        cflags, ldflags, libs = pkg_config("xpm")
        unless libs.nil?
          # Update flags
          @options["cpppath"] << " %s" % [ cflags ]
          @options["extflags"] << " %s %s" % [ ldflags, libs ]

          $defs.push("-DHAVE_X11_XPM_H")
          ret = true
        else
          @options["xpm"] = "no"
        end

        ret
      end
    end

    # Check pkg-config for Xft
    if "yes" == @options["xft"]
      checking_for("X11/Xft/Xft.h") do
        ret = false

        cflags, ldflags, libs = pkg_config("xft")
        unless libs.nil?
          # Update flags
          @options["cpppath"] << " %s" % [ cflags ]
          @options["ldflags"] << " %s %s" % [ ldflags, libs ]
          @options["extflags"] << " %s %s" % [ ldflags, libs ]

          $defs.push("-DHAVE_X11_XFT_XFT_H")
          ret = true
        else
          @options["xft"] = "no"
        end

        ret
      end
    end

    # Xinerama
    if "yes" == @options["xinerama"]
      if have_header("X11/extensions/Xinerama.h")
        @options["ldflags"]  << " -lXinerama"
        @options["extflags"] << " -lXinerama"
      end
    end

    # Check Xrandr
    if "yes" == @options["xrandr"]
      ret = false

      # Pkg-config
      checking_for("X11/extensions/Xrandr.h") do
        cflags, ldflags, libs = pkg_config("xrandr")
        unless libs.nil?
          # Version check (xrandr >= 1.3)
          if try_func("XRRGetScreenResourcesCurrent", libs)
            # Update flags
            @options["cflags"]  << " %s" % [ cflags ]
            @options["ldflags"] << " %s %s" % [ ldflags, libs ]

            $defs.push("-DHAVE_X11_EXTENSIONS_XRANDR_H")

            ret = true
          else
            puts "XRRGetScreenResourcesCurrent couldn't be found"
          end
        end

        @options["xrandr"] = "no" unless ret

        ret
      end
    end

    # Xtest
    if "yes" == @options["xtest"]
      ret = false

      checking_for("X11/extensions/XTest.h") do
        # Check for debian header/lib separation
        if try_func("XTestFakeKeyEvent", "-lXtst")
          @options["extflags"] << " -lXtst"

          $defs.push("-DHAVE_X11_EXTENSIONS_XTEST_H")

          ret = true
        else
          puts "XTestFakeKeyEvent couldn't be found"
        end

        @options["xtest"] = "no" unless ret

        ret
      end
    end

    # Check functions
    FUNCS.each do |f|
      fail("Func #{f} was not found") unless have_func(f)
    end

    # Encoding
    have_func("rb_enc_set_default_internal")

    # Defines
    @defines.each do |k, v|
      $defs.push(format('-D%s="%s"', k, v))
    end

    # Write files
    make_config
    make_header

    # Dump info
    puts <<EOF

#{@defines["PKG_NAME"]} #{@defines["PKG_VERSION"]}
-----------------
Binaries............: #{@options["bindir"]}
Configuration.......: #{@options["configdir"]}
Extension...........: #{@options["extdir"]}

Xpm support.........: #{@options["xpm"]}
Xft support.........: #{@options["xft"]}
Xinerama support....: #{@options["xinerama"]}
XRandR support......: #{@options["xrandr"]}
XTest support.......: #{@options["xtest"]}
Debugging messages..: #{@options["debug"]}

EOF
  end
end # }}}

 ## build {{{
 # Wrapper task for build
 ##

desc("Build all")
task(:build => [:config, PG_SUBTLE, PG_SUBTLEXT])
# }}}

 ## subtle {{{
 # Wrapper task for subtle
 ##

desc("Build subtle")
task(PG_SUBTLE => [:config]) # }}}

 ## subtlext # {{{
 # Wrapper task for subtlext
 ##

desc("Build subtlext")
task(PG_SUBTLEXT => [:config]) # }}}

 ## install {{{
 # Install subtle and components
 ##

desc("Install subtle")
task(:install => [:config, :build]) do
  verbose = (true == RakeFileUtils.verbose)

  # Make install dirs
  FileUtils.mkdir_p(
    [
      @options["bindir"],
      @options["configdir"],
      @options["extdir"],
      @options["mandir"],
      File.join(@options["extdir"], "subtler"),
      File.join(@options["extdir"], "sur")
    ]
  )

  # Install config
  message("INSTALL config\n")
  FileUtils.install(
    File.join("data", @defines["PKG_CONFIG"]),
    @options["configdir"],
    :mode    => 0644,
    :verbose => verbose
  )

  # Install subtle
  message("INSTALL %s\n" % [PG_SUBTLE])
  FileUtils.install(
    PG_SUBTLE,
    @options["bindir"],
    :mode    => 0755,
    :verbose => verbose
  )

  # Install subtlext
  message("INSTALL %s\n" % [PG_SUBTLEXT])
  FileUtils.install(
    PG_SUBTLEXT + ".so",
    @options["extdir"],
    :mode    => 0644,
    :verbose => verbose
  )

  # Get path of sed and ruby interpreter
  sed         = find_executable0("sed")
  interpreter = File.join(
    RbConfig.expand(RbConfig::CONFIG["bindir"]),
    RbConfig::CONFIG["ruby_install_name"]
  )

  # Install subtler
  message("INSTALL subtler\n")
  FileList["data/subtler/*.rb"].collect do |f|
    FileUtils.install(f,
      File.join(@options["extdir"], "subtler"),
      :mode    => 0644,
      :verbose => verbose
    )
  end

  # Install sur
  message("INSTALL sur\n")
  FileList["data/sur/*.rb"].collect do |f|
    FileUtils.install(f,
      File.join(@options["extdir"], "sur"),
      :mode    => 0644,
      :verbose => verbose
    )
  end

  # Install tools
  message("INSTALL tools\n")
  FileList["data/bin/*"].collect do |f|
    FileUtils.install(f, @options["bindir"],
      :mode    => 0755,
      :verbose => verbose
    )

    # Update interpreter name
    `#{sed} -i -e 's#/usr/bin/ruby.*##{interpreter}#' \
      #{File.join(@options["bindir"], File.basename(f))}`
  end

  # Install manpages
  message("INSTALL manpages\n")
  FileList["data/man/*.*"].collect do |f|
    FileUtils.install(f, @options["mandir"],
      :mode    => 0644,
      :verbose => verbose
    )
  end
end # }}}

 ## uninstall {{{
 # Uninstall subtle and components
 ##

desc("Uninstall subtle")
task(:uninstall => [:config]) do
  verbose = (:default != RakeFileUtils.verbose)

  # Uninstall config
  message("UNINSTALL config\n")
  FileUtils.rm_r(
    @options["configdir"],
    :verbose => verbose
  )

  # Uninstall subtle
  message("UNINSTALL %s\n" % [PG_SUBTLE])
  FileUtils.rm(
    File.join(@options["bindir"], PG_SUBTLE),
    :verbose => verbose
  )

  # Uninstall subtlext
  message("UNINSTALL %s\n" % [PG_SUBTLEXT])
  FileUtils.rm(
    File.join(@options["extdir"], PG_SUBTLEXT + ".so"),
    :verbose => verbose
  )

  # Uninstall subtler
  message("UNINSTALL subtler\n")
  FileUtils.rm_r(
    File.join(@options["extdir"], "subtler"),
    :verbose => verbose
  )

  # Uninstall sur
  message("UNINSTALL sur\n")
  FileUtils.rm_r(
    File.join(@options["extdir"], "sur"),
    :verbose => verbose
  )

  # Uninstall tools
  message("UNINSTALL tools\n")
  FileList["data/bin/*"].collect do |f|
    FileUtils.rm(
      File.join(@options["bindir"], File.basename(f)),
      :verbose => verbose
    )
  end

  # Install manpages
  message("UNINSTALL manpages\n")
  FileList["data/man/*.*"].collect do |f|
    FileUtils.rm(
      File.join(@options["mandir"], File.basename(f)),
      :verbose => verbose
    )
  end
end # }}}

 ## help {{{
 # Display help
 ##

desc("Show help")
task(:help => [:config]) do
  puts <<EOF
destdir=PATH       Set intermediate install prefix (current: #{@options["destdir"]})
prefix=PATH        Set install prefix (current: #{@options["prefix"]})
manprefix=PATH     Set install prefix for manpages (current: #{@options["manprefix"]})
bindir=PATH        Set binary directory (current: #{@options["bindir"]})
sysconfdir=PATH    Set config directory (current: #{@options["sysconfdir"]})
datadir=PATH       Set data directory (current: #{@options["datadir"]})
mandir=PATH        Set man directory (current: #{@options["mandir"]})
debug=[yes|no]     Whether to build with debugging messages (current: #{@options["debug"]})
xpm=[yes|no]       Whether to build with Xpm support (current: #{@options["xpm"]})
xft=[yes|no]       Whether to build with Xft support (current: #{@options["xft"]})
xinerama=[yes|no]  Whether to build with Xinerama support (current: #{@options["xinerama"]})
randr=[yes|no]     Whether to build with XRandR support (current: #{@options["xrandr"]})
EOF
end # }}}

 ## rdoc {{{
 # Create rdoc documents
 ##

RDoc::Task.new :rdoc do |rdoc|
  rdoc.rdoc_files.include(
    "data/subtle.rb",
    "src/subtle/ruby.c",
    "src/subtlext/client.c",
    "src/subtlext/color.c",
    "src/subtlext/geometry.c",
    "src/subtlext/gravity.c",
    "src/subtlext/icon.c",
    "src/subtlext/screen.c",
    "src/subtlext/sublet.c",
    "src/subtlext/subtle.c",
    "src/subtlext/subtlext.c",
    "src/subtlext/tag.c",
    "src/subtlext/tray.c",
    "src/subtlext/view.c",
    "src/subtlext/window.c"
  )
  rdoc.options << "-o doc"
  rdoc.title    = "Subtle RDoc Documentation"
end # }}}

# File tasks

# subtle # {{{
SRC_SUBTLE.each do |src|
  out = File.join(@options["builddir"], PG_SUBTLE, File.basename(src).ext("o"))

  file(out => src) do
    compile(src, out, "-D#{PG_SUBTLE.upcase}")
  end
end

file(PG_SUBTLE => OBJ_SUBTLE) do
  silent_sh("#{@options["cc"]} -o #{PG_SUBTLE} #{OBJ_SUBTLE} #{@options["ldflags"]}",
    "LD #{PG_SUBTLE}") do |ok, status|
      ok or fail("Linker failed with status #{status.exitstatus}")
  end
end # }}}

# subtlext # {{{
SRC_SUBTLEXT.each do |src|
  out = File.join(@options["builddir"], PG_SUBTLEXT, File.basename(src).ext("o"))

  file(out => src) do
    compile(src, out, "-D#{PG_SUBTLEXT.upcase} -fPIC")
  end
end

file(PG_SUBTLEXT => OBJ_SUBTLEXT) do
  silent_sh("#{@options["cc"]} -o #{PG_SUBTLEXT}.so #{OBJ_SUBTLEXT} -shared #{@options["extflags"]}",
    "LD #{PG_SUBTLEXT}") do |ok, status|
      ok or fail("Linker failed with status #{status.exitstatus}")
  end
end # }}}

# vim:ts=2:bs=2:sw=2:et:fdm=marker

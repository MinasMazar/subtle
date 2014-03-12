# -*- encoding: utf-8 -*-
#
# @package sur
#
# @file Server functions
# @author Christoph Kappel <unexist@subforge.org>
# @version $Id: data/sur/server.rb,v 3182 2012/02/04 16:39:33 unexist $
#
# This program can be distributed under the terms of the GNU GPLv2.
# See the file COPYING for details.
#

require "rubygems"
require "fileutils"
require "yaml"
require "sinatra"
require "haml"
require "datamapper"
require "digest/md5"
require "uri"
require "net/http"
require "xmlrpc/marshal"

require "subtle/sur/specification"

module Subtle # {{{
  module Sur # {{{
    # Model classes for datamapper
    module Model # {{{

      # Sublet class database model
      class Sublet # {{{
        include DataMapper::Resource

        property(:id,          Serial)
        property(:name,        String, :unique_index => :name)
        property(:contact,     String, :length => 255)
        property(:description, String, :length => 255)
        property(:version,     String, :unique_index => :name)
        property(:date,        DateTime)
        property(:path,        String, :length => 255)
        property(:digest,      String, :length => 255)
        property(:ip,          String, :length => 255)
        property(:required,    String, :required => false)
        property(:downloads,   Integer, :default => 0)
        property(:annotations, Integer, :default => 0)
        property(:created_at,  DateTime)

        has(n, :authors, :model => "Sur::Model::Assoc::Author")
        has(n, :tags,    :model => "Sur::Model::Assoc::Tag")
      end # }}}

      # Tag class database model
      class Tag # {{{
        include DataMapper::Resource

        property(:id,         Serial)
        property(:name,       String, :length => 255, :unique => true)
        property(:created_at, DateTime)

        has(n, :taggings, :model => "Sur::Model::Assoc::Tag")
      end # }}}

      # User class database model
      class User # {{{
        include DataMapper::Resource

        property(:id,         Serial)
        property(:name,       String, :length => 255, :unique => true)
        property(:created_at, DateTime)
      end # }}}

      # Assoc classes for datamapper
      module Assoc

        # Tagging class database model
        class Author # {{{
          include DataMapper::Resource

          property(:id,         Serial)
          property(:user_id,    Integer, :unique_index => :author)
          property(:sublet_id,  Integer, :unique_index => :author)
          property(:created_at, DateTime)

          belongs_to(:user,   :model => "Sur::Model::User")
          belongs_to(:sublet, :model => "Sur::Model::Sublet")
        end # }}}

        # Tag class database model
        class Tag # {{{
          include DataMapper::Resource

          property(:id,         Serial)
          property(:tag_id,     Integer, :unique_index => :tag)
          property(:sublet_id,  Integer, :unique_index => :tag)
          property(:created_at, DateTime)

          belongs_to(:tag,    :model => "Sur::Model::Tag")
          belongs_to(:sublet, :model => "Sur::Model::Sublet")
        end # }}}

        # Tag class database model
        class Annotate # {{{
          include DataMapper::Resource

          property(:id,         Serial)
          property(:sublet_id,  Integer)
          property(:user_id,    Integer)
          property(:created_at, DateTime)

          belongs_to(:sublet, :model => "Sur::Model::Sublet")
          belongs_to(:user,   :model => "Sur::Model::User")
        end # }}}
      end
    end # }}}

    # Server class for repository management
    class Server # {{{
      # Database file
      DATABASE = Dir.pwd + "/repository.db"

      # Repository storage
      REPOSITORY = Dir.pwd + "/repository"

      # Cache file
      CACHE = Dir.pwd + "/cache.yaml"

      # Identi.ca username
      USERNAME = "subtle"

      # Identi.ca password
      PASSWORD = ""

      ## Sur::Server::initialize {{{
      # Create a new Server object
      #
      # @param [Number]  port  Default port
      # @return [Object] New Sur::Server
      #
      # @since 0.0
      #
      # @example
      #   client = Sur::Server.new
      #   => #<Sur::Server:xxx>

      def initialize(port = 4567)
        DataMapper.setup(:default, "sqlite3://" + DATABASE)
        #DataMapper::Model.raise_on_save_failure = true

        # Create database and store
        DataMapper.auto_migrate!  unless File.exists?(DATABASE)
        Dir.mkdir(REPOSITORY)     unless File.exists?(REPOSITORY)

        # Configure sinatra application
        set :port, port
      end # }}}

      ## Sur::Server::run {{{
      # Run sinatra application
      #
      # @since 0.0
      #
      # @example
      #  Sur::Server.new.run
      #  => nil

      def run
        helpers do # {{{
          def build_cache # :nodoc: {{{
            list = []

            # Fetch sublets from database
            Sur::Model::Sublet.all(:order => [ :name, :version.desc ]).each do |s|

              # Collect authors
              authors = []
              Sur::Model::Assoc::Author.all(:sublet_id => s.id).each do |a|
                authors.push(a.user.name)
              end

              # Collect tags
              tags = []
              Sur::Model::Assoc::Tag.all(:sublet_id => s.id).each do |a|
                tags.push(a.tag.name)
              end

              # Create spec
              spec = Sur::Specification.new do |spec|
                spec.name             = s.name
                spec.authors          = authors
                spec.contact          = s.contact
                spec.description      = s.description
                spec.version          = s.version
                spec.date             = s.date
                spec.tags             = tags
                spec.digest           = s.digest
                spec.required_version = s.required
              end

               list.push(spec)
            end

            # Store cache
            yaml = list.to_yaml
            File.open(Sur::Server::CACHE, "w+") do |out|
              YAML::dump(yaml, out)
            end

            puts ">>> Regenerated sublets cache with #{list.size} sublets"
          end # }}}

          def get_all_sublets_in_interval(args) # :nodoc: {{{
            sublets = []

            # Find sublets in interval
            if 2 == args.size
              Sur::Model::Sublet.all(
                  :created_at.gte => Time.at(args[0]),
                  :created_at.lte => Time.at(args[1])
                  ).each do |s|
                sublets << "%s-%s" % [ s.name, s.version ]
              end
            end

            XMLRPC::Marshal.dump_response(sublets)
          end # }}}
        end # }}}

        # Templates

        template :layout do # {{{
          <<EOF
!!! 1.1
%html
  %head
    %title SUR - Subtle User Repository

    %style{:type => "text/css"}
      :sass
        body
          :color #000000
          :background-color #ffffff
          :font-family Verdana,sans-serif
          :font-size 12px
          :margin 0px
          :padding 0px
        h1
          :color #444444
          :font-size 20px
          :margin 0 0 10px
          :padding 2px 10px 1px 0
        h2
          :color #444444
          :font-size 16px
          :margin 0 0 10px
          :padding 2px 10px 1px 0
        input
          :margin-top 1px
          :margin-bottom 1px
          :vertical-align middle
        input[type="text"]
          :border 1px solid #D7D7D7
        input[type="text"]:focus
          :border 1px solid #888866
        input[type="submit"]
          :background-color #F2F2F2
          :border 1px outset #CCCCCC
          :color #222222
        input[type="submit"]:hover
          :background-color #CCCCBB
        a#download:link, a#download:visited, a#download:active
          :color #000000
          :text-decoration none
          :border-bottom 1px dotted #E4E4E4
        a#download:hover
          :color #000000
          :text-decoration none
        a:link, a:visited, a:active
          :color #59554e
          :text-decoration underline
        a:hover
          :text-decoration none
        .center
          :margin 0px auto
        .italic
          :font-style italic
        .gray
          :color #999999
        #box
          :background-color #FCFCFC
          :border 1px solid #eee9de
          :color #505050
          :line-height 1.5em
          :padding 6px
          :margin-bottom 10px
          :margin-right 10px
          :margin-top 10px
          :float left
          :min-width 280px
          :min-height 240px
        #form
          :width 900px
          :padding 10px 0px 10px 0px
        #frame
          :padding 10px
        #left
          :margin-right 10px
          :margin-top 10px
          :float left
          :width 630px
        #right
          :margin-right 10px
          :margin-top 10px
          :float right
          :width 230px
          :text-align right
        #clear
          :clear both

  %body
    #frame
      =yield
EOF
        end # }}}

        template :index do # {{{
          <<EOF
%h2 Sublets
%p
  Small Ruby scripts written in a
  %a{:target => "_parent", :href => "http://en.wikipedia.org/wiki/Domain_Specific_Language"} DSL
  that provides things like system information for the
  %a{:target => "_parent", :href => "http://subtle.subforge.org"} subtle
  panel.

#form
  #left
    %form{:method => "get", :action => "/search"}
      Search:
      %input{:type => "text", :size => 40, :name => "query"}
      %input{:type => "submit", :name => "submit", :value => "Go"}

  #right
    %a{:target => "_parent", :href => "http://subforge.org/wiki/subtle/Specification"} Sublet specification
    |
    %a{:href => "http://sur.subforge.org/sublets"} All sublets

#clear

#box
  %h2 Latest sublets
  %ul
    -@newest.each do |s|
      %li
        %a#download{:href => "http://sur.subforge.org/get/%s" % [ s.digest ] }
          ="%s-%s" % [ s.name, s.version ]
        ="(%s)" % [ s.tags.map { |t| '<a href="/tag/%s">#%s</a>' % [ t.tag.name, t.tag.name ] }.join(", ") ]

#box
  %h2 Most downloaded
  %ul
    -@most.each do |s|
      %li
        %a#download{:href => "http://sur.subforge.org/get/%s" % [ s.digest ] }
          ="%s-%s" % [ s.name, s.version ]
        ="(%d)" % [ s.downloads ]

#box
  %h2 Never downloaded
  %ul
    -@never.each do |s|
      %li
        %a#download{:href => "http://sur.subforge.org/get/%s" % [ s.digest ] }
          ="%s-%s" % [ s.name, s.version ]
        ="(%s)" % [ s.tags.map { |t| '<a href="/tag/%s">#%s</a>' % [ t.tag.name, t.tag.name ] }.join(", ") ]

  %h2 Just broken
  %ul
    -@worst.each do |s|
      %li
        %a#download{:href => "http://sur.subforge.org/get/%s" % [ s.digest ] }
          ="%s-%s" % [ s.name, s.version ]
        ="(%d)" % [ s.annotations ]
EOF
        end # }}}

        template :tag do # {{{
          <<EOF
#box
  %h2
    Sublets tagged with
    %span.italic= @tag

  %ul
    -if !@list.nil?
      -@list.each do |s|
        %li
          %a#download{:href => "/get/%s" % [ s.digest ] }
            ="%s-%s" % [ s.name, s.version ]
          ="(%s)" % [ s.tags.map { |t| '<a href="/tag/%s">#%s</a>' % [ t.tag.name, t.tag.name ] }.join(", ") ]
#clear

.center
  %a{:href => "javascript:history.back()"} Back
EOF
        end # }}}

        template :sublets do # {{{
          <<EOF
#box
  %h2= "All sublets (%d)" % [ @list.size ]

  %ul
    -@list.each do |s|
      %li
        %a#download{:href => "/get/%s" % [ s.digest ] }
          ="%s-%s" % [ s.name, s.version ]
        ="(%s)" % [ s.tags.map { |t| '<a href="/tag/%s">#%s</a>' % [ t.tag.name, t.tag.name ] }.join(", ") ]
        %span.gray= "(%d downloads)" % [ s.downloads ]
#clear

.center
  %a{:href => "javascript:history.back()"} Back
EOF
        end # }}}

        template :search do # {{{
          <<EOF
#box
  %h2
    Search for
    %span.italic= @query

  %ul
    -@list.each do |s|
      %li
        %a#download{:href => "/get/%s" % [ s.digest ] }
          ="%s-%s" % [ s.name, s.version ]
        ="(%s)" % [ s.tags.map { |t| '<a href="/tag/%s">#%s</a>' % [ t.tag.name, t.tag.name ] }.join(", ") ]
        %span.gray= "(%d downloads)" % [ s.downloads ]
#clear

.center
  %a{:href => "javascript:history.back()"} Back
EOF
        end # }}}

        # Handlers

        get "/" do # {{{
          @newest = Sur::Model::Sublet.all(:order => [ :created_at.desc ], :limit => 10)
          @most   = Sur::Model::Sublet.all(:downloads.gte => 0, :order => [ :downloads.desc ], :limit => 10)
          @never  = Sur::Model::Sublet.all(:downloads => 0, :order => [ :created_at.asc ], :limit => 4)
          @worst  = Sur::Model::Sublet.all(:annotations.gt => 0, :order => [ :annotations.asc ], :limit => 4)

          haml(:index)
        end # }}}



        get "/tag/:tag" do # {{{
          @tag  = params[:tag].capitalize
          @list = Sur::Model::Sublet.all(
            Sur::Model::Sublet.tags.tag.name => params[:tag]
          )

          haml(:tag)
        end # }}}

        get "/sublets" do # {{{
          @list = Sur::Model::Sublet.all(:order => [ :name.asc ])

          haml(:sublets)
        end # }}}

        get "/search" do # {{{
          @query = params[:query]
          @list  = Sur::Model::Sublet.all(
            :name.like => params[:query].gsub(/\*/, "%")
          )

          haml(:search)
        end # }}}

        post "/annotate" do # {{{
          if (s = Sur::Model::Sublet.first(:digest => params[:digest]))
            # Find or create user
            u = Sur::Model::User.first_or_create(
              { :name       => params[:user] },
              { :created_at => Time.now      }
            )

            # Create annotation
            Sur::Model::Assoc::Annotate.create(
              :sublet_id  => s.id,
              :user_id    => u.id,
              :created_at => Time.now
            )

            # Increase annotation count
            s.annotations = s.annotations + 1
            s.save

            puts ">>> Added annotation from `#{u.name}` for `#{s.name}'"
          else
            puts ">>> WARNING: Cannot find sublet with digest `#{params[:digest]}`"
            halt 404
          end
        end # }}}

        Sinatra::Application.run!
      end # }}}
    end # }}}
  end # }}}
end # }}}

# vim:ts=2:bs=2:sw=2:et:fdm=marker

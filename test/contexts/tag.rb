#
# @package test
#
# @file Test Subtlext::Tag functions
# @author Christoph Kappel <unexist@subforge.org>
# @version $Id: test/contexts/tag.rb,v 3169 2012/01/03 20:43:30 unexist $
#
# This program can be distributed under the terms of the GNU GPLv2.
# See the file COPYING for details.
#

context 'Tag' do
  TAG_COUNT = 13
  TAG_ID    = 1
  TAG_NAME  = 'terms'

  setup do # {{{
    Subtlext::Tag.first(TAG_ID)
  end # }}}

  asserts 'Check attributes' do # {{{
    TAG_ID == topic.id and TAG_NAME == topic.name
  end # }}}

  asserts 'Get list' do # {{{
    list = Subtlext::Tag.list

    list.is_a?(Array) and TAG_COUNT == list.size and
      Subtlext::Tag.method(:all) == Subtlext::Tag.method(:list)
  end # }}}

  asserts 'Finder' do # {{{
    index  = Subtlext::Tag[TAG_ID]
    string = Subtlext::Tag[TAG_NAME]
    sym    = Subtlext::Tag[TAG_NAME.to_sym]
    all    = Subtlext::Tag.find('.*')
    none   = Subtlext::Tag['abcdef']

    index == string and index == sym and
      all.is_a?(Array) and TAG_COUNT == all.size and
      none.nil?
  end # }}}

  asserts 'First' do # {{{
    index  = Subtlext::Tag.first(TAG_ID)
    string = Subtlext::Tag.first(TAG_NAME)

    index == string
  end # }}}

  asserts 'Equal and compare' do # {{{
    topic.eql?(Subtlext::Tag.first(TAG_ID)) and topic == topic
  end # }}}

  asserts 'Hash and unique' do # {{{
    1 == [ topic, topic ].uniq.size
  end # }}}

  asserts 'Check associations' do # {{{
    clients = topic.clients
    views   = topic.views

    clients.is_a?(Array) and 1 == clients.size and
      views.is_a?(Array) and 1 == views.size
  end # }}}

  asserts 'Convert to string' do # {{{
    TAG_NAME == topic.to_str
  end # }}}

  asserts 'Create new tag' do # {{{
    t = Subtlext::Tag.new('test')
    t.save

    sleep 1

    TAG_COUNT + 1 == Subtlext::Tag.all.size
  end # }}}

  asserts 'Kill a tag' do # {{{
    Subtlext::Tag.first('test').kill

    sleep 1

    TAG_COUNT == Subtlext::Tag.all.size
  end # }}}
end

# vim:ts=2:bs=2:sw=2:et:fdm=marker

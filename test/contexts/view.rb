#
# @package test
#
# @file Test Subtlext::View functions
# @author Christoph Kappel <unexist@subforge.org>
# @version $Id: test/contexts/view.rb,v 3169 2012/01/03 20:43:30 unexist $
#
# This program can be distributed under the terms of the GNU GPLv2.
# See the file COPYING for details.
#

context 'View' do
  VIEW_COUNT = 4
  VIEW_ID    = 0
  VIEW_NAME  = 'terms'
  VIEW_TAG   = :default

  setup do # {{{
    Subtlext::View.first(VIEW_ID)
  end # }}}

  asserts 'Check attributes' do # {{{
    VIEW_ID == topic.id and VIEW_NAME == topic.name
  end # }}}

  asserts 'Get list' do # {{{
    list = Subtlext::View.list

    list.is_a?(Array) and VIEW_COUNT == list.size and
      Subtlext::View.method(:all) == Subtlext::View.method(:list)
  end # }}}

  asserts 'Finder' do # {{{
    index  = Subtlext::View[VIEW_ID]
    string = Subtlext::View[VIEW_NAME]
    sym    = Subtlext::View[VIEW_NAME.to_sym]
    all    = Subtlext::View.find('.*')
    none   = Subtlext::View['abcdef']

    index == string and index == sym and
      all.is_a?(Array) and VIEW_COUNT == all.size and
      none.nil?
  end # }}}

  asserts 'First' do # {{{
    index  = Subtlext::View.first(VIEW_ID)
    string = Subtlext::View.first(VIEW_NAME)

    index == string
  end # }}}

  asserts 'Equal and compare' do # {{{
    topic.eql?(Subtlext::View.first(VIEW_ID)) and topic == topic
  end # }}}

  asserts 'Hash and unique' do # {{{
    1 == [ topic, topic ].uniq.size
  end # }}}

  asserts 'Check associations' do # {{{
    clients = topic.clients
    tags    = topic.tags

    clients.is_a?(Array) and 1 == clients.size and
      tags.is_a?(Array) and 2 == tags.size
  end # }}}

  asserts 'Check icon' do # {{{
    nil == topic.icon
  end # }}}

  asserts 'Check current' do # {{{
    topic.current?
  end # }}}

  asserts 'Convert to string' do # {{{
    VIEW_NAME == topic.to_str
  end # }}}

  asserts 'Create new view' do # {{{
    v = Subtlext::View.new('test')
    v.save

    sleep 1

    5 == Subtlext::View.all.size
  end # }}}

  asserts 'Switch views' do # {{{
    view_next = topic.next
    view_next.jump

    sleep 1

    view_prev = view_next.prev
    topic.jump

    sleep 1

    view_prev == topic
  end # }}}

  asserts 'Add/remove tags' do # {{{
    tag = Subtlext::Tag.all.last

    # Compare tag counts
    before = topic.tags.size
    topic.tag(tag)

    sleep 0.5

    middle1 = topic.tags.size
    topic.tags = [ tag, 'default' ]

    sleep 0.5

    middle2 = topic.tags.size
    topic.untag(tag)

    sleep 0.5

    after = topic.tags.size

    before == middle1 - 1 and 2 == middle2 and
      1 == after and topic.has_tag?(VIEW_TAG)
  end # }}}

  asserts 'Store values' do # {{{
    topic[:test] = 'test'

    'test' == Subtlext::View.current[:test]
  end # }}}

  asserts 'Kill a view' do # {{{
    Subtlext::View.first('test').kill

    sleep 1

    VIEW_COUNT == Subtlext::View.all.size
  end # }}}
end

# vim:ts=2:bs=2:sw=2:et:fdm=marker

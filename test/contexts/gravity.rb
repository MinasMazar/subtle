#
# @package test
#
# @file Test Subtlext::Gravity functions
# @author Christoph Kappel <unexist@subforge.org>
# @version $Id: test/contexts/gravity.rb,v 3169 2012/01/03 20:43:30 unexist $
#
# This program can be distributed under the terms of the GNU GPLv2.
# See the file COPYING for details.
#

context 'Gravity' do
  GRAVITY_COUNT = 30
  GRAVITY_ID    = 12
  GRAVITY_NAME  = 'center'

  setup do # {{{
    Subtlext::Gravity.first(GRAVITY_ID)
  end # }}}

  asserts 'Check attributes' do # {{{
    GRAVITY_ID == topic.id and GRAVITY_NAME == topic.name and
      topic.geometry.is_a? Subtlext::Geometry
  end # }}}

  asserts 'Get list' do # {{{
    list = Subtlext::Gravity.list

    list.is_a?(Array) and GRAVITY_COUNT == list.size and
      Subtlext::Gravity.method(:all) == Subtlext::Gravity.method(:list)
  end # }}}

  asserts 'Finder' do # {{{
    index  = Subtlext::Gravity[GRAVITY_ID]
    string = Subtlext::Gravity[GRAVITY_NAME + '$']
    sym    = Subtlext::Gravity[GRAVITY_NAME.to_sym]
    all    = Subtlext::Gravity.find('.*')
    none   = Subtlext::Gravity['abcdef']

    index == string and index == sym and
      all.is_a?(Array) and GRAVITY_COUNT == all.size and
      none.nil?
  end # }}}

  asserts 'First' do # {{{
    index  = Subtlext::Gravity.first(GRAVITY_ID)
    string = Subtlext::Gravity.first(GRAVITY_NAME)

    index == string
  end # }}}

  asserts 'Equal and compare' do # {{{
    topic.eql?(Subtlext::Gravity.first(GRAVITY_ID)) and topic == topic
  end # }}}

  asserts 'Hash and unique' do # {{{
    1 == [ topic, topic ].uniq.size
  end # }}}

  asserts 'Check associations' do # {{{
    clients = topic.clients

    clients.is_a?(Array) and 1 == clients.size
  end # }}}

  asserts 'Check geometry for screen' do # {{{
    screen = Subtlext::Screen.current
    geom   = topic.geometry_for(screen)

    screen.geometry == geom
  end # }}}

  asserts 'Convert to string' do # {{{
    GRAVITY_NAME == topic.to_str
  end # }}}

  asserts 'Create new gravity' do # {{{
    g = Subtlext::Gravity.new('test', 0, 0, 100, 100)
    g.save

    sleep 0.5

    GRAVITY_COUNT + 1 == Subtlext::Gravity.all.size
  end # }}}

  asserts 'Test tiling' do # {{{
    topic.tiling = :vert
    topic.tiling = :horz
    topic.tiling = nil

    true
  end # }}}

  asserts 'Kill a gravity' do # {{{
    Subtlext::Gravity.first('test').kill

    sleep 0.5

    GRAVITY_COUNT == Subtlext::Gravity.all.size
  end # }}}
end

# vim:ts=2:bs=2:sw=2:et:fdm=marker

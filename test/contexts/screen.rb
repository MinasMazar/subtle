#
# @package test
#
# @file Test Subtlext::Screen functions
# @author Christoph Kappel <unexist@subforge.org>
# @version $Id: test/contexts/screen.rb,v 3169 2012/01/03 20:43:30 unexist $
#
# This program can be distributed under the terms of the GNU GPLv2.
# See the file COPYING for details.
#

context 'Screen' do
  setup do # {{{
    Subtlext::Screen.current
  end # }}}

  asserts 'Check attributes' do # {{{
    0 == topic.id and '0x16+1024+752' == topic.geometry.to_str
  end # }}}

  asserts 'Get list' do # {{{
    list = Subtlext::Screen.list

    list.is_a?(Array) and 1 == list.size and
      Subtlext::Screen.method(:all) == Subtlext::Screen.method(:list)
  end # }}}

  asserts 'Find and compare' do # {{{
    topic == Subtlext::Screen[0]
  end # }}}

  asserts 'Finder' do # {{{
    Subtlext::Screen[0] == Subtlext::Screen.find(
      Subtlext::Geometry.new(100, 100, 100, 100)
    )
  end # }}}

  asserts 'Check current' do # {{{
    topic.current?
  end # }}}

  asserts 'Equal and compare' do # {{{
    topic.eql?(Subtlext::Screen.current) and topic == topic
  end # }}}

  asserts 'Hash and unique' do # {{{
    1 == [ topic, topic ].uniq.size
  end # }}}

  asserts 'Change view' do # {{{
    view1 = topic.view

    sleep 0.5

    topic.view = 'www'

    sleep 0.5

    view2      = topic.view
    topic.view = view1

    sleep 0.5

    view3 = topic.view

    view1 == view3 and 'www' == view2.name
  end # }}}

  asserts 'Convert to string' do # {{{
    '0x16+1024+752' == topic.to_str
  end # }}}
end

# vim:ts=2:bs=2:sw=2:et:fdm=marker

#
# @package test
#
# @file Test Subtlext::Sublet functions
# @author Christoph Kappel <unexist@subforge.org>
# @version $Id: test/contexts/sublet.rb,v 3171 2012/01/03 20:53:09 unexist $
#
# This program can be distributed under the terms of the GNU GPLv2.
# See the file COPYING for details.
#

context 'Sublet' do
  SUBLET_COUNT = 1
  SUBLET_ID    = 0
  SUBLET_NAME  = 'dummy'

  setup do # {{{
    Subtlext::Sublet.first(SUBLET_ID)
  end # }}}

  asserts 'Check attributes' do # {{{
    SUBLET_ID == topic.id and SUBLET_NAME == topic.name
  end # }}}

  asserts 'Get list' do # {{{
    list = Subtlext::Sublet.list

    list.is_a?(Array) and SUBLET_COUNT == list.size and
      Subtlext::Sublet.method(:all) == Subtlext::Sublet.method(:list)
  end # }}}

  asserts 'Finder' do # {{{
    index  = Subtlext::Sublet[SUBLET_ID]
    string = Subtlext::Sublet[SUBLET_NAME]
    sym    = Subtlext::Sublet[SUBLET_NAME.to_sym]
    all    = Subtlext::Sublet.find('.*')
    none   = Subtlext::Sublet['abcdef']

    index == string and index == sym and none.nil?
  end # }}}

  asserts 'First' do # {{{
    index  = Subtlext::Sublet.first(SUBLET_ID)
    string = Subtlext::Sublet.first(SUBLET_NAME)

    index == string
  end # }}}

  asserts 'Update sublet' do # {{{
    topic.update

    true
  end # }}}

  asserts 'Get geometry' do # {{{
    topic.geometry.is_a?(Subtlext::Geometry)
  end # }}}

  asserts 'Equal and compare' do # {{{
    topic.eql?(topic) and topic == topic
  end # }}}

  asserts 'Hash and unique' do # {{{
    1 == [ topic, topic ].uniq.size
  end # }}}

  asserts 'Convert to string' do # {{{
    SUBLET_NAME == topic.to_str
  end # }}}
end

# vim:ts=2:bs=2:sw=2:et:fdm=marker

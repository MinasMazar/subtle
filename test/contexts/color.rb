#
# @package test
#
# @file Test Subtlext::Color functions
# @author Christoph Kappel <unexist@subforge.org>
# @version $Id: test/contexts/color.rb,v 3169 2012/01/03 20:43:30 unexist $
#
# This program can be distributed under the terms of the GNU GPLv2.
# See the file COPYING for details.
#

context 'Color' do
  setup do # {{{
    Subtlext::Color.new '#ff0000'
  end # }}}

  asserts 'Init types' do # {{{
    c1 = Subtlext::Color.new(topic)
    c2 = Subtlext::Color.new([ 255, 0, 0 ])
    c3 = Subtlext::Color.new({ red: 255, green: 0, blue: 0 })

    topic == c1 and c1 == c2 and c1 == c3
  end # }}}

  asserts 'Check attributes' do # {{{
    255 == topic.red and 0 == topic.green and 0 == topic.blue
  end # }}}

  asserts 'Type conversions' do # {{{
    hex  = topic.to_hex
    hash = topic.to_hash
    ary  = topic.to_ary

    '#FF0000' == hex and hash.values == ary
  end # }}}

  asserts 'Equal and compare' do # {{{
    topic.eql?(Subtlext::Color.new('#ff0000')) and topic == topic
  end # }}}

  asserts 'Hash and unique' do # {{{
    1 == [ topic, topic ].uniq.size
  end # }}}

  asserts 'Convert to string' do # {{{
    topic.to_str.match(/<>#[0-9]+<>/)
  end # }}}
end

# vim:ts=2:bs=2:sw=2:et:fdm=marker

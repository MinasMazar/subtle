#
# @package test
#
# @file Test Subtlext::Client functions
# @author Christoph Kappel <unexist@subforge.org>
# @version $Id: test/contexts/client.rb,v 3169 2012/01/03 20:43:30 unexist $
#
# This program can be distributed under the terms of the GNU GPLv2.
# See the file COPYING for details.
#

context 'Client' do
  CLIENT_COUNT = 1
  CLIENT_ID    = 0
  CLIENT_NAME  = 'xterm'
  CLIENT_TAG   = :default

  setup do # {{{
    Subtlext::Client.current
  end # }}}

  asserts 'Check attributes' do # {{{
    CLIENT_NAME == topic.name and
      CLIENT_NAME == topic.instance and CLIENT_NAME == topic.klass.downcase
  end # }}}

  asserts 'Get list' do # {{{
    list = Subtlext::Client.list

    list.is_a?(Array) and CLIENT_COUNT == list.size and
      Subtlext::Client.method(:all) == Subtlext::Client.method(:list)
  end # }}}

  asserts 'Finder' do # {{{
    index  = Subtlext::Client[CLIENT_ID]
    string = Subtlext::Client[CLIENT_NAME]
    sym    = Subtlext::Client[CLIENT_NAME.to_sym]
    all    = Subtlext::Client.find('.*')
    none   = Subtlext::Client['abcdef']

    index == string and index == sym and none.nil?
  end # }}}

  asserts 'First' do # {{{
    index  = Subtlext::Client.first(CLIENT_ID)
    string = Subtlext::Client.first(CLIENT_NAME)

    index == string
  end # }}}

  asserts 'Equal and compare' do # {{{
    topic.eql?(Subtlext::Client.current) and topic == topic
  end # }}}

  asserts 'Hash and unique' do # {{{
    1 == [ topic, topic ].uniq.size
  end # }}}

  asserts 'Convert to string' do # {{{
    CLIENT_NAME == topic.to_str
  end # }}}

  asserts 'Send button' do # {{{
    nil == topic.send_button(1)
  end # }}}

  asserts 'Send key' do # {{{
    nil == topic.send_key('a')
  end # }}}

  asserts 'Add/remove flags' do # {{{
    p = lambda { |c, is, toggle|
      ret = []

      2.times do |i|
        ret << c.send(is)
        c.send toggle
        sleep 0.5
      end

      ret << c.send(is)

      ret
    }

    # Check flags
    expected = [ false, true, false ]
    results  = [
      :full ,:float, :stick, :resize,
      :urgent, :zaphod, :fixed, :borderless
    ].map { |flag|
      p.call(topic, "is_#{flag}?".to_sym, "toggle_#{flag}".to_sym)
    }

    results.all? { |r| r == expected }
  end # }}}

  asserts 'Add/remove tags' do # {{{
    tag = Subtlext::Tag.all.last

    # Compare tag counts
    before = topic.tags.size
    topic.tag tag

    sleep 0.5

    # Compare tag counts
    middle1 = topic.tags.size
    topic.tags = [ tag, 'default' ]

    sleep 0.5

    # Compare tag counts
    middle2 = topic.tags.size
    topic.untag tag

    sleep 0.5

    # Compare tag counts
    after = topic.tags.size

    before == middle1 - 1 and 2 == middle2 and
      before == after and topic.has_tag?(CLIENT_TAG)
  end # }}}

  asserts 'Set/get gravity' do # {{{
    topic.gravity = 12

    sleep 0.5

    index = topic.gravity == Subtlext::Gravity.first(12)
    topic.gravity = :center

    sleep 0.5

    sym = topic.gravity == Subtlext::Gravity.first(:center)
    topic.gravity = 'center'

    sleep 0.5

    string = topic.gravity == Subtlext::Gravity.first('center')
    topic.gravity = Subtlext::Gravity.first(:center)

    sleep 0.5

    string = topic.gravity == Subtlext::Gravity.first(:center)

    # Set gravity on www view
    topic.toggle_stick
    topic.gravity = { :www => :left }

    sleep 0.5

    # Jump to www vew and check gravity
    Subtlext::View.first(:www).jump
    left = topic.gravity == Subtlext::Gravity.first(:left)

    index and sym and string and left
  end # }}}

  asserts 'Get Screen' do # {{{
    topic.screen.is_a?(Subtlext::Screen)
  end # }}}

  asserts 'Store values' do # {{{
    topic[:test] = 'test'

    'test' == Subtlext::Client.current[:test]
  end # }}}

  asserts 'Kill a client' do # {{{
    topic.kill

    sleep 1

    0 == Subtlext::Client.all.size
  end # }}}
end

# vim:ts=2:bs=2:sw=2:et:fdm=marker

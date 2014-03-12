#
# @package test
#
# @file Test Subtlext::Subtle functions
# @author Christoph Kappel <unexist@subforge.org>
# @version $Id: test/contexts/subtle_init.rb,v 3131 2011/11/15 20:43:06 unexist $
#
# This program can be distributed under the terms of the GNU GPLv2.
# See the file COPYING for details.
#

context 'Subtle - Init' do
  COLORS_COUNT = 48

  asserts 'Check running' do # {{{
    Subtlext::Subtle.running?
  end # }}}

  asserts 'Check colors' do # {{{
    Subtlext::Subtle.colors.is_a?(Hash) and
      COLORS_COUNT == Subtlext::Subtle.colors.size
  end # }}}

  asserts 'Check font' do # {{{
    '-*-*-*-*-*-*-14-*-*-*-*-*-*-*' == Subtlext::Subtle.font
  end # }}}

  asserts 'Check spawn' do # {{{
    if (xterm = find_executable0('xterm')).nil?
      raise 'xterm not found in path'
    else
      Subtlext::Subtle.spawn("#{xterm} -display :10")
    end

    sleep 1

    1 == Subtlext::Client.all.size
  end # }}}
end

# vim:ts=2:bs=2:sw=2:et:fdm=marker

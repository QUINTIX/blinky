/*
  Red Black Trees
  (C) 1999  Andrea Arcangeli <andrea@suse.de>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

-----------------------------------------------------------------------

  To use rbtrees you'll have to implement your own insert and search cores.
  This will avoid us to use callbacks and to drop drammatically performances.
  I know it's not the cleaner way,  but in C (not in C++) to get
  performances and genericity...

  Some example of insert and search follows here. The search is a plain
  normal search over an ordered tree. The insert instead must be implemented
  int two steps: as first thing the code must insert the element in
  order as a red leaf in the tree, then the support library function
  rb_insert_color() must be called. Such function will do the
  not trivial work to rebalance the rbtree if necessary.

-----------------------------------------------------------------------
*/

#ifndef	RB_TREE_H
#define	RB_TREE_H

#include <stddef.h>

struct redblack_node {
    struct redblack_node *rb_parent;
    int rb_color;
#define	QRB_RED		0
#define	QRB_BLACK	1
    struct redblack_node *rb_right;
    struct redblack_node *rb_left;
};

struct rb_root {
    struct redblack_node *redblack_node;
};

#define QRB_ROOT	(struct rb_root) { NULL, }

/*
 *
 */
extern void rb_insert_color(struct redblack_node *, struct rb_root *);

/*
 *
 */
extern void rb_erase(struct redblack_node *, struct rb_root *);

static inline void
rb_link_node(struct redblack_node *node, struct redblack_node *parent,
	     struct redblack_node **rb_link)
{
    node->rb_parent = parent;
    node->rb_color = QRB_RED;
    node->rb_left = node->rb_right = NULL;

    *rb_link = node;
}

#endif /* RB_TREE_H */

/* TyTools - public domain
   Niels Martignène <niels.martignene@protonmail.com>
   https://koromix.dev/tytools

   This software is in the public domain. Where that dedication is not
   recognized, you are granted a perpetual, irrevocable license to copy,
   distribute, and modify this file as you see fit.

   See the LICENSE file for more details. */

#include "test_libty.h"
#include "../../src/libty/optline.h"

static void test_optline_empty(void)
{
    {
        ty_optline_context ctx;
        ty_optline_init(&ctx, NULL, 0);

        ASSERT(!ty_optline_next_option(&ctx));
        ASSERT(!ty_optline_consume_non_option(&ctx));
    }
}

static void test_optline_short(void)
{
    {
        char *args[] = {"-f"};
        ty_optline_context ctx;
        ty_optline_init(&ctx, args, TY_COUNTOF(args));

        ASSERT_STR_EQUAL(ty_optline_next_option(&ctx), "-f");
        ASSERT(!ty_optline_next_option(&ctx));
        ASSERT(!ty_optline_consume_non_option(&ctx));
    }

    {
        char *args[] = {"-foo", "-b"};
        ty_optline_context ctx;
        ty_optline_init(&ctx, args, TY_COUNTOF(args));

        ASSERT_STR_EQUAL(ty_optline_next_option(&ctx), "-f");
        ASSERT_STR_EQUAL(ty_optline_next_option(&ctx), "-o");
        ASSERT_STR_EQUAL(ty_optline_next_option(&ctx), "-o");
        ASSERT_STR_EQUAL(ty_optline_next_option(&ctx), "-b");
        ASSERT(!ty_optline_next_option(&ctx));
        ASSERT(!ty_optline_consume_non_option(&ctx));
    }
}

static void test_optline_long(void)
{
    {
        char *args[] = {"--foobar"};
        ty_optline_context ctx;
        ty_optline_init(&ctx, args, TY_COUNTOF(args));

        ASSERT_STR_EQUAL(ty_optline_next_option(&ctx), "--foobar");
        ASSERT(!ty_optline_next_option(&ctx));
        ASSERT(!ty_optline_consume_non_option(&ctx));
    }

    {
        char *args[] = {"--foo", "--bar"};
        ty_optline_context ctx;
        ty_optline_init(&ctx, args, TY_COUNTOF(args));

        ASSERT_STR_EQUAL(ty_optline_next_option(&ctx), "--foo");
        ASSERT_STR_EQUAL(ty_optline_next_option(&ctx), "--bar");
        ASSERT(!ty_optline_next_option(&ctx));
        ASSERT(!ty_optline_consume_non_option(&ctx));
    }
}

static void test_optline_mixed(void)
{
    {
        char *args[] = {"--foo", "-bar"};
        ty_optline_context ctx;
        ty_optline_init(&ctx, args, TY_COUNTOF(args));

        ASSERT_STR_EQUAL(ty_optline_next_option(&ctx), "--foo");
        ASSERT_STR_EQUAL(ty_optline_next_option(&ctx), "-b");
        ASSERT_STR_EQUAL(ty_optline_next_option(&ctx), "-a");
        ASSERT_STR_EQUAL(ty_optline_next_option(&ctx), "-r");
        ASSERT(!ty_optline_next_option(&ctx));
        ASSERT(!ty_optline_consume_non_option(&ctx));
    }

    {
        char *args[] = {"-foo", "--bar", "-FOO"};
        ty_optline_context ctx;
        ty_optline_init(&ctx, args, TY_COUNTOF(args));

        ASSERT_STR_EQUAL(ty_optline_next_option(&ctx), "-f");
        ASSERT_STR_EQUAL(ty_optline_next_option(&ctx), "-o");
        ASSERT_STR_EQUAL(ty_optline_next_option(&ctx), "-o");
        ASSERT_STR_EQUAL(ty_optline_next_option(&ctx), "--bar");
        ASSERT_STR_EQUAL(ty_optline_next_option(&ctx), "-F");
        ASSERT_STR_EQUAL(ty_optline_next_option(&ctx), "-O");
        ASSERT_STR_EQUAL(ty_optline_next_option(&ctx), "-O");
        ASSERT(!ty_optline_next_option(&ctx));
        ASSERT(!ty_optline_consume_non_option(&ctx));
    }
}

static void test_optline_value(void)
{
    {
        char *args[] = {"-f", "bar"};
        ty_optline_context ctx;
        ty_optline_init(&ctx, args, TY_COUNTOF(args));

        ASSERT_STR_EQUAL(ty_optline_next_option(&ctx), "-f");
        ASSERT_STR_EQUAL(ty_optline_get_value(&ctx), "bar");
        ASSERT(!ty_optline_next_option(&ctx));
        ASSERT(!ty_optline_consume_non_option(&ctx));
    }

    {
        char *args[] = {"-fbar"};
        ty_optline_context ctx;
        ty_optline_init(&ctx, args, TY_COUNTOF(args));

        ASSERT_STR_EQUAL(ty_optline_next_option(&ctx), "-f");
        ASSERT_STR_EQUAL(ty_optline_get_value(&ctx), "bar");
        ASSERT(!ty_optline_next_option(&ctx));
        ASSERT(!ty_optline_consume_non_option(&ctx));
    }

    {
        char *args[] = {"--foo=bar"};
        ty_optline_context ctx;
        ty_optline_init(&ctx, args, TY_COUNTOF(args));

        ASSERT_STR_EQUAL(ty_optline_next_option(&ctx), "--foo");
        ASSERT_STR_EQUAL(ty_optline_get_value(&ctx), "bar");
        ASSERT(!ty_optline_next_option(&ctx));
        ASSERT(!ty_optline_consume_non_option(&ctx));
    }

    {
        char *args[] = {"--foo", "bar"};
        ty_optline_context ctx;
        ty_optline_init(&ctx, args, TY_COUNTOF(args));

        ASSERT_STR_EQUAL(ty_optline_next_option(&ctx), "--foo");
        ASSERT_STR_EQUAL(ty_optline_get_value(&ctx), "bar");
        ASSERT(!ty_optline_next_option(&ctx));
        ASSERT(!ty_optline_consume_non_option(&ctx));
    }

    {
        char *args[] = {"bar", "--foo"};
        ty_optline_context ctx;
        ty_optline_init(&ctx, args, TY_COUNTOF(args));

        ASSERT_STR_EQUAL(ty_optline_next_option(&ctx), "--foo");
        ASSERT(!ty_optline_get_value(&ctx));
        ASSERT(!ty_optline_next_option(&ctx));
        ASSERT_STR_EQUAL(ty_optline_consume_non_option(&ctx), "bar");
        ASSERT(!ty_optline_consume_non_option(&ctx));
    }
}

static void test_optline_positional(void)
{
    {
        char *args[] = {"foo", "bar"};
        ty_optline_context ctx;
        ty_optline_init(&ctx, args, TY_COUNTOF(args));

        ASSERT_STR_EQUAL(ty_optline_consume_non_option(&ctx), "foo");
        ASSERT_STR_EQUAL(ty_optline_consume_non_option(&ctx), "bar");
        ASSERT(!ty_optline_next_option(&ctx));
        ASSERT(!ty_optline_consume_non_option(&ctx));
    }

    {
        char *args[] = {"foo", "--foobar", "bar"};
        ty_optline_context ctx;
        ty_optline_init(&ctx, args, TY_COUNTOF(args));

        ty_optline_next_option(&ctx);
        ty_optline_next_option(&ctx);
        ASSERT_STR_EQUAL(ty_optline_consume_non_option(&ctx), "foo");
        ASSERT_STR_EQUAL(ty_optline_consume_non_option(&ctx), "bar");
        ASSERT(!ty_optline_next_option(&ctx));
        ASSERT(!ty_optline_consume_non_option(&ctx));
    }

    {
        char *args[] = {"foobar", "--", "foo", "--bar"};
        ty_optline_context ctx;
        ty_optline_init(&ctx, args, TY_COUNTOF(args));

        ty_optline_next_option(&ctx);
        ASSERT_STR_EQUAL(ty_optline_consume_non_option(&ctx), "foobar");
        ASSERT_STR_EQUAL(ty_optline_consume_non_option(&ctx), "foo");
        ASSERT_STR_EQUAL(ty_optline_consume_non_option(&ctx), "--bar");
        ASSERT(!ty_optline_next_option(&ctx));
        ASSERT(!ty_optline_consume_non_option(&ctx));
    }

    {
        char *args[] = {"foo", "FOO", "foobar", "--", "bar", "BAR", "barfoo", "BARFOO"};
        ty_optline_context ctx;
        ty_optline_init(&ctx, args, TY_COUNTOF(args));

        ty_optline_next_option(&ctx);
        ASSERT_STR_EQUAL(ty_optline_consume_non_option(&ctx), "foo");
        ASSERT_STR_EQUAL(ty_optline_consume_non_option(&ctx), "FOO");
        ASSERT_STR_EQUAL(ty_optline_consume_non_option(&ctx), "foobar");
        ASSERT_STR_EQUAL(ty_optline_consume_non_option(&ctx), "bar");
        ASSERT_STR_EQUAL(ty_optline_consume_non_option(&ctx), "BAR");
        ASSERT_STR_EQUAL(ty_optline_consume_non_option(&ctx), "barfoo");
        ASSERT_STR_EQUAL(ty_optline_consume_non_option(&ctx), "BARFOO");
        ASSERT(!ty_optline_next_option(&ctx));
        ASSERT(!ty_optline_consume_non_option(&ctx));
    }
}

static void test_optline_complex(void)
{
    {
        char *args[] = {"--foo1", "bar", "fooBAR", "-foo2", "--foo3=BAR", "-fbar",
                        "--", "FOOBAR", "--", "--FOOBAR"};
        ty_optline_context ctx;
        ty_optline_init(&ctx, args, TY_COUNTOF(args));

        ASSERT_STR_EQUAL(ty_optline_next_option(&ctx), "--foo1");
        ASSERT_STR_EQUAL(ty_optline_get_value(&ctx), "bar");
        ASSERT_STR_EQUAL(ty_optline_next_option(&ctx), "-f");
        ASSERT_STR_EQUAL(ty_optline_next_option(&ctx), "-o");
        ASSERT(!ty_optline_get_value(&ctx));
        ASSERT_STR_EQUAL(ty_optline_next_option(&ctx), "-o");
        ASSERT_STR_EQUAL(ty_optline_next_option(&ctx), "-2");
        ASSERT_STR_EQUAL(ty_optline_next_option(&ctx), "--foo3");
        ASSERT_STR_EQUAL(ty_optline_get_value(&ctx), "BAR");
        ASSERT_STR_EQUAL(ty_optline_next_option(&ctx), "-f");
        ASSERT_STR_EQUAL(ty_optline_get_value(&ctx), "bar");
        ASSERT(!ty_optline_next_option(&ctx));
        ASSERT_STR_EQUAL(ty_optline_consume_non_option(&ctx), "fooBAR");
        ASSERT_STR_EQUAL(ty_optline_consume_non_option(&ctx), "FOOBAR");
        ASSERT_STR_EQUAL(ty_optline_consume_non_option(&ctx), "--");
        ASSERT(!ty_optline_next_option(&ctx));
        ASSERT_STR_EQUAL(ty_optline_consume_non_option(&ctx), "--FOOBAR");
        ASSERT(!ty_optline_next_option(&ctx));
        ASSERT(!ty_optline_consume_non_option(&ctx));
    }
}

static void test_optline_argv(void)
{
    {
        ty_optline_context ctx;
        ty_optline_init_argv(&ctx, 0, NULL);

        ASSERT(!ty_optline_next_option(&ctx));
        ASSERT(!ty_optline_consume_non_option(&ctx));
    }

    {
        char *argv[] = {"foo"};
        ty_optline_context ctx;
        ty_optline_init_argv(&ctx, TY_COUNTOF(argv), argv);

        ASSERT(!ty_optline_next_option(&ctx));
        ASSERT(!ty_optline_consume_non_option(&ctx));
    }

    {
        char *argv[] = {"foo", "--bar"};
        ty_optline_context ctx;
        ty_optline_init_argv(&ctx, TY_COUNTOF(argv), argv);

        ASSERT_STR_EQUAL(ty_optline_next_option(&ctx), "--bar");
        ASSERT(!ty_optline_next_option(&ctx));
        ASSERT(!ty_optline_consume_non_option(&ctx));
    }
}

void test_optline(void)
{
    test_optline_empty();
    test_optline_short();
    test_optline_long();
    test_optline_mixed();
    test_optline_value();
    test_optline_positional();
    test_optline_complex();
    test_optline_argv();
}

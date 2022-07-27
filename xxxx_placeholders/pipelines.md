---
title: "Various Designs for Pipelines"
document: DxxxxR0
date: today
audience: EWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
---

# Various Designs for Pipelines

[@P2011R0] proposed a new, non-overloadable binary operator `|>` such that the expression

::: bq
```cpp
x |> f(y)
```
:::

was defined to be evaluated as:

::: bq
```cpp
f(x, y)
```
:::

without any intermediate `f(y)` expression. The rules of this operator were fairly simple: the right-hand side had to be a call expression, and the left-hand expression was inserted as the first argument into the right-hand call.

But there are other potential designs for such an operator. The goal of this paper is to go over all of the possibilities and attempt to weigh their pros and cons. It would also be useful background to read the JavaScript proposal for this operator [@javascript.pipeline], which both provides a lot of rationale for the feature and goes through a similar exercise.

In short, there are four potential designs. I'll first present all four and then discuss the pros and cons of each: [Left-Threading](#left-threading), [Inverted Invocation](#inverted-invocation), [Placeholder](#placeholder), and [Language Bind](#placeholder-lambda).


## Left-Threading

The left-threading model was what was proposed in [@P2011R0] (and is used by Elixir). In `x |> e`, `e` has to be a call expression of the form `f(args...)` and the evaluation of this operator is a rewrite which places the operand on the left-hand of the pipeline as the first call argument on the right-hand side. This is similar to how member function invocation works (especially in a deducing `this` world).

For example:

|Code|Evaluation|
|-|-|
|`x |> f(y)`|`f(x, y)`|
|`x |> f()`|`f(x)`|
|`x |> f`|ill-formed, because `f` is not a call expression|
|`x |> f + g`|ill-formed, because `f + g` is not a call expression|

## Inverted Invocation

In the inverted invocation model (which is used by F#, Julia, Elm, and OCaml), `|>` is an inverted call invocation. `x |> f` is defined as `f(x)`. The right-hand side can be arbitrarily involved, and `|>` would be sufficiently low precedence to allow this flexibility:

|Code|Evaluation|
|-|-|
|`x |> f(y)`|`f(y)(x)`|
|`x |> f()`|`f()(x)`|
|`x |> f`|`f(x)`|
|`x |> f + g`|`(f + g)(x)`|

## Placeholder

In the placeholder model (which is used by Hack), the right-hand side of `|>` is an arbitrary expression that must contain at least one placeholder. In Hack, that placeholder is <code>$$</code>. But for the purposes of this paper, I'm going to use `%`. The pipeline operator evaluates as if replacing all instances of the placeholder with the left-hand argument:

|Code|Evaluation|
|-|-|
|`x |> f`|ill-formed, no placeholder|
|`x |> f(y)`|ill-formed, no placeholder|
|`x |> f(%, y)`|`f(x, y)`|
|`x |> f(y, %)`|`f(y, x)`|
|`x |> y + %`|`y + x`|
|`x |> f(y) + g(%)`|`f(y) + g(x)`|
|`x |> co_await %`|`co_await x`|
|`x |> f(1, %, %)`|`f(1, x, x)`|

## Language Bind

Consider again the expression `x |> f(y, %)` above. What role does the `f(y, %)` part serve? I don't want to call it a sub-expression since it's not technically a distinct operand here. But conceptually, it at least kind of is. And as such the role that it serves here is quite similar to:

::: bq
```cpp
std::bind(f, y, _1)(x)
```
:::

With two very notable differences:

* The `bind` expression is limited to solely functions, which the placeholder pipeline is not (as illustrated earlier). But more than that, the `bind` expression is limited to the kinds of functions we can pass as parameters, which `f` need not be (e.g. `std::apply` or `std::visit`, or any other function template)
* The `bind` expression has to capture any additional arguments (the bound arguments) because, as a library facility, it is not knowable when those arguments will actually be used. How expensive is capturing `f` and `y`? But with the placeholder expression, we don't need to capture anything, since we know we're immediately evaluating the expression.

That said, the not-quite-expression `f(y, %)` is conceptually a lot like a unary function.

With that in mind, the language bind model is the inverted invocation model except also introducing the ability to use placeholders to introduce a language bind (sort of like partial application). That is:

|Code|Evaluation|
|-|-|
|`x |> f`|`f(x)`|
|`x |> f(y)`|`f(y)(x)`|
|`x |> f(%, y)`|`f(x, y)`|
|`x |> f(y, %)`|`f(y, x)`|
|`x |> y + %`|`y + x`|
|`x |> f(y) + g(%)`|`f(y) + g(x)`|
|`x |> f + g`|`(f + g)(x)`|
|`x |> co_await %`|`co_await x`|
|`x |> f(1, %, %)`|`f(1, x, x)`|

# Comparing the Designs

Now let's try to determine which of these we should pursue.

## Rejecting the Inverted Application Model

Of these, the inverted invocation model (or the F# model) the simplest to understand, specify, and implement. However, for C++ in particular, it is also the least useful.

It actually does happen to still work for Ranges:

::: bq
```cpp
// this expression
r |> views::transform(f) |> views::filter(g)

// evaluates as
views::filter(g)(views::transform(f)(r))
```
:::

Nobody would write the latter code today (probably), but it is valid - because `views::transform(f)` and `views::filter(g)` do give you unary function objects. But in order for that to be the case, additional library work has to be done - and it's precisely that library work that we wrote [@P2011R0] to avoid.

Without ready-made unary functions, we'd have to write lambdas, and our lambdas are not exactly terse:

::: bq
```cpp
r |> [=](auto&& r){ return views::transform(FWD(r); f); }
  |> [=](auto&& r){ return views::filter(FWD(r); g); }
```
:::

Nor are our binders which additionally work on only a restricted set of potential callables:

::: bq
```cpp
r |> std::bind_back(views::transform, f)
  |> std::bind_back(views::filter, g)
```
:::

And none of this avoids the unnecessary capture that this model would require. As such, it's the easiest to reject.

## To Placehold or Not To Placehold

With placeholders, we get significantly more flexibility. There are situations where the the parameter we want to pipe into isn't the first parameter of the function. There are multiple such examples in the standard library:

* In Ranges, `r1 |> zip_transform(f, %, r2)`. The function is the first parameter, but range pipelines are going to be built up of ranges.
* Similarly, `some_tuple |> apply(f, %)` and `some_variant |> visit(f, %)` fall into the same boat: the function is the first parameter, but the "subject" of the operation is what you want to put on the left.

Additionally there are going to be cases where the expression we want to pipe the left-hand argument into isn't a function call, the fact that it could be anything allows for a wide variety of possibilities. Or that you could pipe into the expression multiple times.

Those are large plusses.

On the flip side, using a placeholder is necessarily more verbose than the left-threading model, by just a single character for unary adaptors (`|> f()` vs `|> f(%)`, the parentheses would be required in the left-threading model) and by three characters per invocation (`%, ` assuming you put a space between function arguments) for multiple arguments:

::: bq
```cpp
// left-threading
r |> views::transform(f)
  |> views::filter(g)
  |> views::reverse();

// placeholder
r |> views::transform(%, f)
  |> views::filter(%, g)
  |> views::reverse(%);
```
:::

Now, while the placeholder allows more flexibility in expressions, for certain kinds of libraries (like Ranges and the new Sender/Receiver model), the common case (indeed, the overwhelmingly common case) is to pipe into the first argument. With the left-threading model, we have no syntactic overhead as a result and don't lose out on much. With the placeholder model, we'd end up with a veritable sea of `meow(%)` and `meow(%, arg)`s.

Since left-threading is the one thing we can do today (by way of `|` and library machinery), it would arguably be more accurate to say that the placeholder model is four characters per call longer than the status quo:

::: bq
```cpp
// status quo
| views::transform(f)

// left-threading
|> views::transform(f)

// placeholder
|> views::transform(%, f)
```
:::

Flexibility vs three extra characters may seem like a silly comparison, but ergonomics matters, and we do have libraries that are specifically designed around first-parameter-passing. It would be nice to not have to pay more syntax when we don't need to.

On the whole though the argument seems to strongly favor placeholders, and if anything exploring a special case of the pipeline operator that does left-threading only and has a more restrictive right-hand side to avoid potential bugs. That might still allow the best of both worlds.

A recent [Conor Hoekstra talk](https://youtu.be/NiferfBvN3s?t=4861) has a nice example that I'll present multiple different ways (in all cases, I will not use the `|` from Ranges).

With left-threading, the example looks like:

::: bq
```cpp
auto filter_out_html_tags(std::string_view sv) -> std::string {
  auto angle_bracket_mask =
    sv |> std::views::transform([](char c){ return c == '<' or c == '>'; });

  return std::views::zip_transform(
      std::logical_or(),
      angle_bracket_mask,
      angle_bracket_mask |> rv::scan_left(std::not_equal_to{})
    |> std::views::zip(sv)
    |> std::views::filter([](auto t){ return not std::get<0>(t); })
    |> std::views::values()
    |> std::ranges::to<std::string>();
}
```
:::

Notably here we run into both of the limitations of left-threading: we need to pipe into a parameter other than the first and we need to pipe more than once.  That requires introducing a new named variable, which is part of what this facility is trying to avoid the need for. This is not a problem for either of the placeholder-using models, as we'll see shortly.

With the placeholder-mandatory model, we don't need that temporary, since we can select which parameters of `zip_transform` to pipe into, and indeed we can pipe twice (I'll have more to say about nested placeholders later):

::: bq
```cpp
auto filter_out_html_tags(std::string_view sv) -> std::string {
  return sv
    |> std::views::transform(%, [](char c){ return c == '<' or c == '>'; })
    |> std::views::zip_transform(std::logical_or{}, %, % |> rv::scan_left(%, std::not_equal_to{}))
    |> std::views::zip(%, sv)
    |> std::views::filter(%, [](auto t){ return not std::get<0>(t); })
    |> std::views::values(%)
    |> std::ranges::to<std::string>(%);
}
```
:::

With the language bind model, we can omit the two uses of `(%)` for the two unary range adaptors:

::: bq
```cpp
auto filter_out_html_tags(std::string_view sv) -> std::string {
  return sv
    |> std::views::transform(%, [](char c){ return c == '<' or c == '>'; })
    |> std::views::zip_transform(std::logical_or{}, %, % |> rv::scan_left(%, std::not_equal_to{}))
    |> std::views::zip(%, sv)
    |> std::views::filter(%, [](auto t){ return not std::get<0>(t); })
    |> std::views::values
    |> std::ranges::to<std::string>;
}
```
:::

And with the introduction of a dedicated operator for left-threading, say `\>`, we can omit four more instances of placeholder:

::: bq
```cpp
auto filter_out_html_tags(std::string_view sv) -> std::string {
  return sv
    \> std::views::transform([](char c){ return c == '<' or c == '>'; })
    |> std::views::zip_with(std::logical_or{}, %, % \> rv::scan_left(std::not_equal_to{}))
    \> std::views::zip(sv)
    \> std::views::filter([](auto t){ return not std::get<0>(t); })
    |> std::views::values
    |> std::ranges::to<std::string>;
}
```
:::

The difference the various placeholder models is about counting characters. For unary functions, can we write `x |> f` or do we have to write `x |> f(%)`? And then for left-threading, do we have to write `x |> f(%, y)` or can we avoid the placeholder with a dedicated `x \> f(y)`? Overall, the last solution (language bind with `\>`) is 18 characters shorter than the placeholder solution, simply by removing what is arguably syntactic noise.

To be honest though, regardless of those 18 characters, the thing that annoys me the most in this example is the lambda. More on that later.

## Placeholder or Language Bind

These are the two most similar models, so let's just compare them against each other using a representative example:

::: cmptable
### Placeholder
```cpp
auto v =
  r |> views::transform(%, f)
    |> views::filter(%, g)
    |> views::reverse(%)
    |> ranges::to<std::vector>(%);
```

### Language Bind
```cpp
auto v =
  r |> views::transform(%, f)
    |> views::filter(%, g)
    |> views::reverse
    |> ranges::to<std::vector>;
```
:::

When the range adaptor is binary (as in `transform` or `filter` or many others), the two are equivalent. We use a placeholder (`%` in this case) for the left-hand side and then provide the other argument manually. No additional binding occurs.

But when the range adaptor is unary, in the placeholder (Hack) model, we still have to use a placeholder. Because we always have to use a placeholder, that's the model. But in the language bind model, a unary adaptor is already a unary function, so there's no need to use language bind to produce one. It just works. In the case where we already have a unary function, the language bind model is three characters shorter - no need to write the `(%)`.

Consider this alternative example, which would be the same syntax in both models:

::: bq
```cpp
auto squares() -> std::generator<int> {
    std::views::iota(0)
        |> std::views::transform(%, [](int i){ return i * i; })
        |> co_yield std::ranges::elements_of(%);
}
```
:::

Admittedly not the most straightforward way to write this function, but it works as an example and helps demonstrate the utility and flexibility of placeholders. Now, let's talk about what this example means in the respective models.

In the placeholder model, this is a straightforward rewrite into a different expression - because the placeholder model is always a rewrite into a different expression. Even if that expression is a `co_yield`.

But in the language bind model, this becomes a little fuzzier. If we say that `co_yield std::ranges::elements_of(%)` is effectively a language bind (even if we side-step the question of captures since we know we're immediately evaluating), that sort of has to imply that the `co_yield` happens in the body of some new function right? But `co_yield` can't work like that, it has to be invoked from `squares` and not from any other internal function. It's not like we actually need to construct a lambda to evaluate this expression, but it does break the cleanliness of saying that this is just inverted function invocation.

Language bind is a more complex model than placeholders and requires a little hand-waving around what exactly we're doing, for a benefit of three characters per unary function call. Is it worth it?

# Placeholder Lambdas

One reason I consider the language bind model attractive, despite the added complexity (both in having to handle `x |> f` in addition to `x |> f(%)` and also having to hand-wave around what `x |> co_yield %` means) is that it also offers a path towards placeholder lambdas. Allow me an aside.

There are basically three approaches that languages take to lambdas (some languages do more than one of these).

1. Direct language support for partial application
2. Placeholder expressions
3. Abbreviated function declarations

I'll illustrate what I mean here by using a simple example: how do you write a lambda that is a unary predicate which checks if its parameter is a negative integer?

For the languages that support partial application, that looks like:

|Expression|Language|
|-|-|
|`(<0)`|Haskell|
|`(<) 0`|F#|

For the languages that provide abbreviated function declarations, we basically have a section that introduces names followed by a body that uses those names:

|Expression|Language|
|-|-|
|`|e| e < 0`|Rust|
|`e -> e < 0`|Java|
|`e => e < 0`|C#, JavaScript, Scala|
|`\e -> e < 0`|Haskell|
|`{ |e| e < 0 }`|Ruby|
|`{ e in e < 0 }`|Swift|
|`{ e -> e < 0 }`|Kotlin
|`fun e -> e < 0`|F#, OCaml|
|`lambda e: e < 0`|Python|
|`fn e -> e < 0 end`|Elixir|
|`[](int e){ return e < 0; }`|C++|
|`func(e int) bool { return e < 0 }`|Go|

On the plus side, C++ is not the longest.

But the interesting case I wanted to discuss here is those languages that support placeholder expressions:

|Expression|Language|
|-|-|
|`_ < 0`|Scala|
|`_1 < 0`|Boost.Lambda (and other Boost libraries)|
|`#(< % 0)`{.x}|Clojure|
|`&(&1 < 0)`|Elixir|
|`{ $0 < 0 }`{.x}|Swift|
|`{ it < 0 }`|Kotlin|

There's a lot of variety in these placeholder lambdas. Some languages number their parameters and let you write whatever you want (Swift starts numbering at `0`, Elixir at `1`, Clojure also at `1` but also provides `%` as a shorthand), Kotlin only provides the special variable `it` to be the first parameter and has no support if you want others.

Scala is unique in that it only provides `_`, but that placeholder refers to a different parameter on each use. So `_ > _` is a binary predicate that checks if the first parameter is greater than the second.

Now, for this particular example, we also have library solutions available to us, and have for quite some time. There are several libraries *just in Boost* that allow for either `_1 < 0` or `_ < 0` to mean the same thing as illustrated above (Boost.HOF's `_` behaves similar to Scala - at least to the extent that is possible in a library). Having placeholder lambdas is quite popular precisely because there's no noise; when writing a simple expression, having to deal with the ceremony of introducing names and dealing with the return type is excessive. For instance, to implement [@P2321R1]'s `zip`, you need to dereference all the iterators in a tuple:

::: cmptable
### Regular Lambda
```cpp
tuple_transform([](auto& it) -> decltype(auto) {
    return *it;
}, current_)
```

### Placeholder Lambda
```cpp
tuple_transform(*_, current_)
```
:::

The C++ library solutions are fundamentally limited to operators though. You can make `_1 == 0` work, but you can't really make `_1.id() == 0` or `f(_1)` work. As a language feature, having member functions work is trivial. But having non-member functions work is... not.

In the table of languages that support placeholder lambdas, four of them have _bounded_ expressions: there is punctuation to mark where the lambda expression begins and ends. This is due to a fundamental ambiguity: what does `f(%)` mean? It could either mean invoking `f` with a unary lambda that returns its argument (i.e. `f(e => e)`) or it could mean a unary lambda that invokes `f` on its argument (i.e. `e => f(e)`). How do you know which one is which?

Scala, which does not have bounded placeholder expressions, takes an interesting direction here:

|Expression|Meaning|
|-|-|
|`f(_)`|`e => f(e)`|
|`f(_, x)`|`e => f(e, x)`|
|`f(_ + 2)`|`e => f(e + 2)`|
|`f(1, 2, g(_))`|`f(1, 2, e => g(e))`|
|`f(1, _)`|`e => f(1, e)`|
|`f(1, _ + 1)`|`f(1, e => e + 1)`|

I'm pretty sure we wouldn't want to go that route in C++, where we come up with some rule for what constitutes the bounded expression around the placeholder.

But also more to the point, when the placeholder expression refers to (odr-uses) a variable, we need to capture it, and we need to know _how_ to capture it.

So a C++ approach to placeholder lambdas might be: a *lambda-introducer*, followed by some token to distinguish it from a regular lambda (e.g. `:` or `=>`), followed by a placeholder expression. That is, the placeholder lambda for the negative example might be `[]: %1 < 0` or just `[]: % < 0`. At 9 characters, this is substantially shorter than the C++ lambda we have to write today (26 characters), and this is about as short as a C++ lambda gets (the dereference example would be 7 characters as compared to 46). And while it's longer than the various library approaches (Boost.HOF's `_ < 0` is just 5), it would have the flexibility to do anything. Probably good enough.

Alternatively could do something closer to Elixir and wrap the placeholder lambda expression, so something like: `[] %(%1 < 0)`.

For more on the topic of placeholder lambdas in C++, consider `vector<bool>`'s blog posts "Now I Am Become Perl" [@vector-bool] and his macro-based solution to the problem [@vector-bool-macro].

Getting back to the topic, when I talked originally about:

::: bq
```cpp
x |> f(y, %)  // means f(y, x)
```
:::

I noted that `f(y, %)` kind of serves as language bind expression, which itself justifies having the model support `x |> f`. But it may also justify splitting the above expression in two:

::: bq
```cpp
// some version of lambda placeholder syntax
auto fy = [=] %(f(y, %1));
auto fy = [=] %(f(y, %));
auto fy = [=]: f(y, %1);
auto fy = [=]: f(y, %);

// at which point it's just a lambda
fy(x)         // evaluates to f(y, x)
```
:::

Although note that with the lambda, you have to capture `y`, whereas with the pipeline expression, there was no intermediate step.

---
references:
    - id: pipeline-minutes
      citation-label: pipeline-minutes
      title: "P2011R1 Telecon - September 2020"
      author:
        - family: EWG
      issued:
        - year: 2020
      URL: https://wiki.edg.com/bin/view/Wg21summer2020/EWG-P2011R1-10-Sep-2020
    - id: vector-bool
      citation-label: vector-bool
      title: "Now I Am Become Perl"
      author:
        - family: Colby Pike
      issued:
        - year: 2018
      URL: https://vector-of-bool.github.io/2018/10/31/become-perl.html
    - id: vector-bool-macro
      citation-label: vector-bool-macro
      title: "A Macro-Based Terse Lambda Expression"
      author:
        - family: Colby Pike
      issued:
        - year: 2021
      URL: https://vector-of-bool.github.io/2021/04/20/terse-lambda-macro.html
    - id: javascript.pipeline
      citation-label: javascript.pipeline
      title: "Proposals for `|>` operator"
      author:
          - family: TC39
      issued:
          - year: 2019
      URL: https://github.com/tc39/proposal-pipeline-operator/
---
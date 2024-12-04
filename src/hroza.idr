-- Prelude --

-- append : T[] x T -> T[]
-- append s t = [...s, t]

-- head : T[] -> T[]
-- head [...s, t] = s

-- last : T[] -> T
-- last [...s, t] = t

-- prepend : T x T[] -> T[]
-- prepend t s = [t, ...s]

-- first : T[] -> T
-- first [t, ...s] = t

-- tail : T[] -> T[]
-- tail [t, ...s] = s

-- concat : T[] x T[] -> T[]
-- concat s t = [...s, ...t]

-- ∘ : (B -> C) x (A -> B) x A -> C
-- (g ∘ f) a = g (f a)

-- fold : (U x T -> U) x U x T[] -> U
-- fold f u []        = u
-- fold f u [t, ...s] = fold f (f u t) s

-- filter : (T -> Bool) x T[] -> T[]
-- filter p = fold f []
--   where f a t = if p t then [...a, t] else a

-- map : (T -> U) x T[] -> U[]
-- map m = fold f []
--   where f a = append a ∘ m

-- flat : T[][] -> T[]
-- flat = fold concat []

-- flatmap : (T -> U[]) x T[] -> U[]
-- flatmap m = fold f []
--   where f a = concat a ∘ m

-- ⊗ : T[] x U[] -> (T x U)[]
-- t ⊗ u = map m t
--   where m a = map n u
--     where n b = [a, b]

-- every : (T -> Bool) x T[] -> Bool
-- every p = fold f true
--   where f a = a and ∘ p

-- any : (T -> Bool) x T[] -> Bool
-- any p = fold f false
--   where f a = a or ∘ p

-- is_in : T x T[] -> Bool
-- t is_in s = any (t ==) s

--------
record Rule Symbol where
  input : (Maybe Symbol)[][]
  output : (Maybe Symbol)[]

Grid = Symbol[]
Locus = Nat

match : Grid -> (Locus, Rule) -> Bool
match g (u, r) = every p ∘ enumerate r.input
  where p (v, Nothing) = True
        p (v,  Just i) = g[u + v] is_in i

changes : Grid -> (Locus x Rule) -> Locus[]
changes g [u, r] = filter p ∘ map m r.output
  where m [v, o] = [u + v, o]
        p [v, o] = every (o !=) [null, g[v]]

apply : Grid -> (Locus x Rule) -> Grid
apply g = fold f g ∘ changes g
  where f g [v, o] = g[v] <- o

matches : Grid -> Rule[] -> (Locus x Rule)[]
matches g = filter (match g) ∘ (g ⊗)

--------

one : Rule[] x Grid -> Grid
one rs g = fold apply g ∘ append [] ∘ first ∘ shuffle ∘ matches g rs

prl : Rule[] x Grid -> Grid
prl rs g = fold apply g ∘ matches g rs

all : Rule[] x Grid -> Grid
all rs g = fold apply g ∘ fold f [] ∘ shuffle ∘ matches g rs
  where f a m = if first m is_in map first a then a else concat a m

--------

no_limit : (Grid -> Grid) x Grid -> Grid[]
no_limit n g = if r == g then [] else prepend r ∘ no_limit n r
  where r = n g

limit : UInt x (Grid -> Grid) x Grid -> Grid[]
limit 0 _ _ = []
limit i n g = if r == g then [] else prepend r ∘ limit (i-1) n r
  where r = n g

sequence : (Grid -> Grid[])[] x Grid -> Grid[]
sequence ns g = tail ∘ fold f [g] ns
  where f gs n = concat gs ∘ n ∘ last gs

markov : (Grid -> Grid[])[] x Grid -> Grid[]
markov ns g = if gs == [] then [] else concat gs ∘ markov ns ∘ last gs
  where gs = fold f [] ns
    where f gs n = if gs != [] then gs else n g

--------

seq_snake = sequence [
  no_limit ∘ one ["WBB" -> "PER"],
  limit 10 ∘ one ["RBB" -> "EER"],
  markov [
    no_limit ∘ one [
      "RBB" -> "GGU",
      "EEG" -> "GGG",
      "PEG" -> "BBP",
    ],
    no_limit ∘ all [
      "G" -> "E",
      "U" -> "R",
    ],
    no_limit ∘ all [
      "R" -> "P",
      "P" -> "R",
    ]
  ]
]

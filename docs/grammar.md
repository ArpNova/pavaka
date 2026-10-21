$$
\begin{align}
    [\text{Prog}] &\to [\text{Stmt}]^* \\
    [\text{Stmt}] &\to
    \begin{cases}
        \text{exit}([\text{Expr}]); \\
        \text{print}([\text{Expr}]^+); \\
        \text{let}\space\text{vector<}[\text{Type}]\text{>}\space\text{ident}\space(=\space\{([\text{Expr}](,[\text{Expr}])*)?\})?; \\
        \text{let}\space\text{string}\space\text{ident} = [\text{Expr}]; \\
        \text{let}\space\text{char}\space\text{ident} = [\text{Expr}]; \\
        \text{let}\space\text{int}?\space\text{ident} = [\text{Expr}]; \\
        \text{ident}.\text{push\_back}([\text{Expr}]); \\
        \text{ident}.\text{pop\_back}(); \\
        \text{ident}[[\text{Expr}]] = [\text{Expr}]; \\
        \text{ident} = [\text{Expr}]; \\
        \text{if}\space([\text{Expr}])[\text{Scope}][\text{IfPred}]? \\
        \text{while}\space([\text{Expr}])[\text{Scope}] \\
        \text{break}; \\
        \text{continue}; \\
        [\text{Scope}]
    \end{cases} \\
    [\text{Type}] &\to \text{int} \mid \text{char} \mid \text{string} \\
    [\text{IfPred}] &\to
    \begin{cases}
        \text{else}\space\text{if}\space([\text{Expr}])[\text{Scope}][\text{IfPred}]? \\
        \text{else}\space[\text{Scope}]
    \end{cases} \\
    [\text{Scope}] &\to \{[\text{Stmt}]^*\} \\
    [\text{Expr}] &\to
    \begin{cases}
        [\text{Term}] \\
        [\text{BinExpr}]
    \end{cases} \\
    [\text{BinExpr}] &\to
    \begin{cases}
        [\text{Expr}] * [\text{Expr}] & \text{prec} = 4 \\
        [\text{Expr}] / [\text{Expr}] & \text{prec} = 4 \\
        [\text{Expr}] + [\text{Expr}] & \text{prec} = 3 \\
        [\text{Expr}] - [\text{Expr}] & \text{prec} = 3 \\
        [\text{Expr}] < [\text{Expr}] & \text{prec} = 2 \\
        [\text{Expr}] <= [\text{Expr}] & \text{prec} = 2 \\
        [\text{Expr}] > [\text{Expr}] & \text{prec} = 2 \\
        [\text{Expr}] >= [\text{Expr}] & \text{prec} = 2 \\
        [\text{Expr}] == [\text{Expr}] & \text{prec} = 1 \\
        [\text{Expr}] != [\text{Expr}] & \text{prec} = 1 \\
    \end{cases} \\ 
    [\text{Term}] &\to
    \begin{cases}
        \text{int\_lit} \\
        \text{string\_lit} \\
        \text{char\_lit} \\
        \text{ident} \\
        \text{ident}[[\text{Expr}]] \\
        \text{ident}.\text{size}() \\
        \$([\text{Expr}]) \\
        ([\text{Expr}])
    \end{cases}
\end{align}
$$
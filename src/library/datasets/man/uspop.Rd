% File src/library/datasets/man/uspop.Rd
% Part of the R package, https://www.R-project.org
% Copyright 1995-2007 R Core Team
% Distributed under GPL 2 or later

\name{uspop}
\docType{data}
\alias{uspop}
\title{Populations Recorded by the US Census}
\description{
  This data set gives the population of the United States (in millions)
  as recorded by the decennial census for the period 1790--1970.
}
\usage{uspop}
\format{A time series of 19 values.}
\source{
  McNeil, D. R. (1977)
  \emph{Interactive Data Analysis}.
  New York: Wiley.
}
% FIXME: missing \references{}
\examples{
require(graphics)
plot(uspop, log = "y", main = "uspop data", xlab = "Year",
     ylab = "U.S. Population (millions)")
}
\keyword{datasets}

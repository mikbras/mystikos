#######################################################################
# To run this container without Mystikos:
#     docker run -p 5050:5050 `docker build -q .`
#######################################################################

FROM mcr.microsoft.com/dotnet/core/aspnet:3.1-alpine AS base
RUN apk add --no-cache icu-libs
WORKDIR /app
EXPOSE 5050

ENV ASPNETCORE_URLS=http://*:5050

FROM mcr.microsoft.com/dotnet/core/sdk:3.1-buster AS build
WORKDIR /src
COPY . .
RUN dotnet publish "/src/webapp.csproj" -c Release -o /app/build --self-contained true -r alpine-x64

FROM base AS final
WORKDIR /app
COPY --from=build /app/build .
WORKDIR /
ENTRYPOINT ["/app/webapp"]
